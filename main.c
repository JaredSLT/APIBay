#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h> // _write
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>
#include <intrin.h> // For __cpuidex
#include <errno.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET sock_t;
#define close_sock(s) closesocket(s)
#define WRITE_FD _write
#define STDOUT_FD 1
#define STDERR_FD 2
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/uio.h>
#include <fcntl.h>
#if defined(__linux__)
#include <sys/epoll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#endif
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define close_sock(s) close(s)
#define WRITE_FD write
#define STDOUT_FD STDOUT_FILENO
#define STDERR_FD STDERR_FILENO
#endif
#if defined(__x86_64__)
#include <immintrin.h>
#endif
#if defined(__arm__) || defined(__aarch64__)
#include <arm_neon.h>
#endif
#if defined(__powerpc64__)
#include <altivec.h>
#endif
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif
#if defined(__aarch64__) && defined(__ARM_FEATURE_SVE)
#include <arm_sve.h>
#endif
#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#ifndef _WIN32
static inline int strnicmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int d = tolower((unsigned char)s1[i]) - tolower((unsigned char)s2[i]);
        if (d != 0) return d;
    }
    return 0;
}
#endif
static inline int simd_strnicmp(const char *restrict s1, const char *restrict s2, size_t n) {
#if defined(__x86_64__) && defined(__AVX2__)
    while (n >= 32) {
        __m256i a = _mm256_loadu_si256((const __m256i *)s1);
        __m256i b = _mm256_loadu_si256((const __m256i *)s2);
        __m256i mask = _mm256_set1_epi8(0x20);
        __m256i A_m1 = _mm256_set1_epi8('A' - 1);
        __m256i Z_p1 = _mm256_set1_epi8('Z' + 1);
        __m256i is_upper_a = _mm256_and_si256(_mm256_cmpgt_epi8(a, A_m1), _mm256_cmplt_epi8(a, Z_p1));
        __m256i a_lo = _mm256_or_si256(a, _mm256_and_si256(is_upper_a, mask));
        __m256i is_upper_b = _mm256_and_si256(_mm256_cmpgt_epi8(b, A_m1), _mm256_cmplt_epi8(b, Z_p1));
        __m256i b_lo = _mm256_or_si256(b, _mm256_and_si256(is_upper_b, mask));
        __m256i eq = _mm256_cmpeq_epi8(a_lo, b_lo);
        uint32_t m = _mm256_movemask_epi8(eq);
        if (m != 0xFFFFFFFFu) {
            int pos = __builtin_ctz(~m);
            return tolower((unsigned char)s1[pos]) - tolower((unsigned char)s2[pos]);
        }
        s1 += 32;
        s2 += 32;
        n -= 32;
    }
#endif
    int d = 0;
    for (size_t i = 0; i < n; i++) {
        d = tolower((unsigned char)s1[i]) - tolower((unsigned char)s2[i]);
        if (d != 0) return d;
    }
    return 0;
}
static int hex_val[256];
static inline size_t parse_hex(const char *str, size_t len) {
    size_t val = 0;
    for (size_t i = 0; i < len; i++) {
        int v = hex_val[(unsigned char)str[i]];
        if (v < 0) break;
        val = val * 16 + v;
    }
    return val;
}
static inline void *memmem_scalar(const void *restrict haystack, size_t haystacklen, const void *restrict needle, size_t needlelen) {
    if (needlelen == 0) return (void *)haystack;
    if (haystacklen < needlelen) return NULL;
    const unsigned char *h = haystack;
    const unsigned char *n = needle;
    int bad_char_skip[256];
    for (int i = 0; i < 256; i++) bad_char_skip[i] = -1;
    for (int i = 0; i < needlelen; i++) bad_char_skip[n[i]] = i;
    int skip;
    for (int scan = 0; scan <= haystacklen - needlelen; scan += skip) {
        __builtin_prefetch(&h[scan + 32]);
        skip = 0;
        for (int j = needlelen - 1; j >= 0; j--) {
            if (h[scan + j] != n[j]) {
                skip = j - bad_char_skip[h[scan + j]];
                if (skip < 1) skip = 1;
                break;
            }
        }
        if (skip == 0) return (void *)(h + scan);
    }
    return NULL;
}
#if defined(__x86_64__) && defined(__AVX2__)
static inline void *memmem_avx2(const void *restrict haystack, size_t haystacklen, const void *restrict needle, size_t needlelen) {
    if (needlelen == 0) return (void *)haystack;
    if (haystacklen < needlelen) return NULL;
    if (needlelen < 32) return memmem_scalar(haystack, haystacklen, needle, needlelen);
    const unsigned char *h = haystack;
    const unsigned char *n = needle;
    __m256i first = _mm256_set1_epi8(n[0]);
    __m256i last = _mm256_set1_epi8(n[needlelen - 1]);
    size_t pos = 0;
    while (pos + needlelen + 30 <= haystacklen) {
        __builtin_prefetch(h + pos + 64);
        __m256i chunk_first = _mm256_loadu_si256((__m256i *)(h + pos));
        __m256i eq_first = _mm256_cmpeq_epi8(first, chunk_first);
        uint32_t mask_first = _mm256_movemask_epi8(eq_first);
        __m256i chunk_last = _mm256_loadu_si256((__m256i *)(h + pos + needlelen - 1));
        __m256i eq_last = _mm256_cmpeq_epi8(last, chunk_last);
        uint32_t mask_last = _mm256_movemask_epi8(eq_last);
        uint32_t mask = mask_last & mask_first;
        while (mask != 0) {
            int bitpos = __builtin_ctz(mask);
            if (memcmp(h + pos + bitpos, n, needlelen) == 0) {
                return (void *)(h + pos + bitpos);
            }
            mask &= ~(1u << bitpos);
        }
        pos += 32;
    }
    return memmem_scalar(h + pos, haystacklen - pos, needle, needlelen);
}
#if defined(__AVX512F__)
static inline void *memmem_avx512(const void *restrict haystack, size_t haystacklen, const void *restrict needle, size_t needlelen) {
    if (needlelen == 0) return (void *)haystack;
    if (haystacklen < needlelen) return NULL;
    if (needlelen < 64) return memmem_avx2(haystack, haystacklen, needle, needlelen);
    const unsigned char *h = haystack;
    const unsigned char *n = needle;
    __m512i first = _mm512_set1_epi8(n[0]);
    __m512i last = _mm512_set1_epi8(n[needlelen - 1]);
    size_t pos = 0;
    while (pos + needlelen + 62 <= haystacklen) {
        __builtin_prefetch(h + pos + 128);
        __m512i chunk_first = _mm512_loadu_si512((__m512i *)(h + pos));
        __m512i eq_first = _mm512_cmpeq_epi8(first, chunk_first);
        uint64_t mask_first = _mm512_movemask_epi8(eq_first);
        __m512i chunk_last = _mm512_loadu_si512((__m512i *)(h + pos + needlelen - 1));
        __m512i eq_last = _mm512_cmpeq_epi8(last, chunk_last);
        uint64_t mask_last = _mm512_movemask_epi8(eq_last);
        uint64_t mask = mask_last & mask_first;
        while (mask != 0) {
            int bitpos = __builtin_ctzll(mask);
            if (memcmp(h + pos + bitpos, n, needlelen) == 0) {
                return (void *)(h + pos + bitpos);
            }
            mask &= ~(1ULL << bitpos);
        }
        pos += 64;
    }
    return memmem_avx2(h + pos, haystacklen - pos, needle, needlelen);
}
#endif
#endif
typedef void* (*memmem_fn)(const void*, size_t, const void*, size_t);
static memmem_fn memmem_opt = NULL;
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
static inline uint64_t rdtsc(void) {
#ifdef _MSC_VER
    return __rdtsc();
#else
    unsigned int low, high;
    __asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t)high << 32) | low;
#endif
}
static double g_tsc_freq = 0.0;
static uint64_t g_tsc_base = 0;
static long long g_time_base = 0;
#elif defined(__aarch64__)
static double g_tsc_freq = 0.0;
static uint64_t g_tsc_base = 0;
static long long g_time_base = 0;
static inline uint64_t rdtsc(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}
static inline double get_freq(void) {
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return (double)freq;
}
#elif defined(__powerpc64__)
static double g_tsc_freq = 0.0;
static uint64_t g_tsc_base = 0;
static long long g_time_base = 0;
static inline uint64_t rdtsc(void) {
    uint64_t val;
    uint32_t tbu0, tbl, tbu1;
    do {
        __asm__ __volatile__ ("mfspr %0,269" : "=r"(tbu0));
        __asm__ __volatile__ ("mfspr %0,268" : "=r"(tbl));
        __asm__ __volatile__ ("mfspr %0,269" : "=r"(tbu1));
    } while (tbu0 != tbu1);
    val = ((uint64_t)tbu0 << 32) | tbl;
    return val;
}
#elif defined(__riscv)
static double g_tsc_freq = 0.0;
static uint64_t g_tsc_base = 0;
static long long g_time_base = 0;
static inline uint64_t rdtsc(void) {
    uint64_t val;
    __asm__ volatile("rdtime %0" : "=r"(val));
    return val;
}
#endif
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
static inline void cpuid(int info[4], int leaf) {
#ifdef __GNUC__
    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "0" (leaf)
    );
#else
    __cpuid(info, leaf);
#endif
}
static inline int is_intel(void) {
    static int cached = -1;
    if (cached == -1) {
        int info[4];
        cpuid(info, 0);
        cached = (info[1] == 0x756e6547 && info[3] == 0x49656e69 && info[2] == 0x6c65746e);
    }
    return cached;
}
static inline int is_amd(void) {
    static int cached = -1;
    if (cached == -1) {
        int info[4];
        cpuid(info, 0);
        cached = (info[1] == 0x68747541 && info[3] == 0x69746e65 && info[2] == 0x444d4163);
    }
    return cached;
}
static inline bool has_aesni(void) {
    static int cached = -1;
    if (cached == -1) {
        int info[4];
        cpuid(info, 1);
        cached = (info[2] & (1 << 25)) != 0;
    }
    return cached;
}
static inline bool has_avx2(void) {
    static int cached = -1;
    if (cached == -1) {
        int info[4];
        cpuid(info, 0);
        if (info[0] < 7) {
            cached = 0;
            return cached;
        }
        cpuid(info, 7);
        cached = (info[1] & (1 << 5)) != 0;
    }
    return cached;
}
static inline bool has_avx512(void) {
    static int cached = -1;
    if (cached == -1) {
        int info[4];
        cpuid(info, 0);
        if (info[0] < 7) {
            cached = 0;
            return cached;
        }
        cpuid(info, 7);
        cached = (info[1] & (1 << 16)) != 0;
    }
    return cached;
}
#endif
static inline long long current_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER cnt, g_freq;
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&cnt);
    return (cnt.QuadPart * 1000000000LL) / g_freq.QuadPart;
#else
#if defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || defined(__powerpc64__) || defined(__riscv)
    static bool has_invariant_tsc = false;
    if (unlikely(g_tsc_freq == 0.0)) {
#if defined(__x86_64__) || defined(__i386__)
        // Check for invariant TSC
        int ext_info[4];
        cpuid(ext_info, 0x80000000);
        unsigned max_ext = (unsigned)ext_info[0];
        if (max_ext >= 0x80000007) {
            cpuid(ext_info, 0x80000007);
            has_invariant_tsc = (ext_info[3] & (1 << 8)) != 0;
        }
        if (has_invariant_tsc) {
            int info[4];
            cpuid(info, 0);
            int max_leaf = info[0];
            // Try CPUID 0x15 for TSC frequency
            if (max_leaf >= 0x15) {
                cpuid(info, 0x15);
                unsigned denom = (unsigned)info[0];
                unsigned numer = (unsigned)info[1];
                unsigned long long crystal_hz = (unsigned)info[2];
                if (crystal_hz == 0) {
                    // Get family and model
                    int cpu_info[4];
                    cpuid(cpu_info, 1);
                    unsigned base_family = (cpu_info[0] >> 8) & 0xf;
                    unsigned base_model = (cpu_info[0] >> 4) & 0xf;
                    unsigned ext_family = (cpu_info[0] >> 20) & 0xff;
                    unsigned ext_model = (cpu_info[0] >> 16) & 0xf;
                    unsigned family = (base_family == 0xF) ? base_family + ext_family : base_family;
                    unsigned model = (family == 0xF || family == 0x6) ? (ext_model << 4) + base_model : base_model;
                    if (is_intel()) {
                        if (family == 6) {
                            if (model >= 0x97) { // Alder Lake and later
                                crystal_hz = 38400000ULL;
                            } else if (model >= 0x3D) { // Broadwell to Rocket Lake
                                crystal_hz = 24000000ULL;
                            } else if (model >= 0x0F) { // Core 2 to Haswell
                                crystal_hz = 25000000ULL;
                            } else {
                                crystal_hz = 25000000ULL; // Default for older
                            }
                            if (model == 0x55) crystal_hz = 25000000ULL;
                        }
                    } else if (is_amd()) {
                        crystal_hz = 25000000ULL;
                    }
                }
                if (numer != 0 && denom != 0 && crystal_hz != 0) {
                    g_tsc_freq = (double)crystal_hz * (double)numer / (double)denom;
                }
            }
            if (g_tsc_freq == 0.0 && max_leaf >= 0x16) {
                cpuid(info, 0x16);
                g_tsc_freq = (unsigned)info[0] * 1000000.0; // Base frequency in MHz
            }
            if (g_tsc_freq == 0.0) {
                // Parse brand string for frequency
                int brand[12];
                cpuid(brand, 0x80000002);
                cpuid(brand + 4, 0x80000003);
                cpuid(brand + 8, 0x80000004);
                char brand_str[48];
                memcpy(brand_str, brand, sizeof(brand_str));
                brand_str[47] = '\0';
                char *ghz_pos = strstr(brand_str, "GHz");
                char *freq_start = NULL;
                double freq = 0.0;
                if (ghz_pos) {
                    freq_start = ghz_pos - 1;
                    while (freq_start > brand_str && (isdigit(*freq_start) || *freq_start == '.')) {
                        freq_start--;
                    }
                    freq_start++;
                    freq = atof(freq_start);
                    g_tsc_freq = freq * 1000000000.0;
                } else {
                    char *mhz_pos = strstr(brand_str, "MHz");
                    if (mhz_pos) {
                        freq_start = mhz_pos - 1;
                        while (freq_start > brand_str && (isdigit(*freq_start) || *freq_start == '.')) {
                            freq_start--;
                        }
                        freq_start++;
                        freq = atof(freq_start);
                        g_tsc_freq = freq * 1000000.0;
                    }
                }
            }
        }
#elif defined(__aarch64__)
        has_invariant_tsc = true; // Assume on ARM64
        g_tsc_freq = get_freq();
#elif defined(__powerpc64__)
        has_invariant_tsc = false;
#elif defined(__riscv)
        has_invariant_tsc = false;
#endif
        if (g_tsc_freq == 0.0) {
            // Fallback calibration
            uint64_t tsc_start = rdtsc();
            struct timespec ts_start, ts_end;
            clock_gettime(CLOCK_MONOTONIC, &ts_start);
            usleep(1000000);
            clock_gettime(CLOCK_MONOTONIC, &ts_end);
            long long calib_start_ns = (ts_start.tv_sec * 1000000000LL) + ts_start.tv_nsec;
            long long calib_end_ns = (ts_end.tv_sec * 1000000000LL) + ts_end.tv_nsec;
            uint64_t tsc_end = rdtsc();
            long long delta_ns = calib_end_ns - calib_start_ns;
            if (delta_ns > 0 && (tsc_end > tsc_start)) {
                g_tsc_freq = (double)(tsc_end - tsc_start) / (delta_ns * 1e-9);
                g_tsc_base = tsc_start;
                g_time_base = calib_start_ns;
            }
        }
    }
    if (g_tsc_freq > 0.0 && has_invariant_tsc) {
        uint64_t ticks = rdtsc() - g_tsc_base;
        return g_time_base + (long long)((double)ticks / g_tsc_freq * 1e9);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (ts.tv_sec * 1000000000LL) + ts.tv_nsec;
    }
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000000LL) + ts.tv_nsec;
#endif
#endif
}
#define INITIAL_BUFFER_SIZE 16777216UL
#define MAX_QUERY_LEN 1024
#define MAX_ESCAPED_SIZE ((MAX_QUERY_LEN * 3) + 1)
static uint8_t url_safe[256] = {
    ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1,
    ['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,
    ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1, ['G'] = 1, ['H'] = 1, ['I'] = 1, ['J'] = 1,
    ['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1, ['O'] = 1, ['P'] = 1, ['Q'] = 1, ['R'] = 1, ['S'] = 1, ['T'] = 1,
    ['U'] = 1, ['V'] = 1, ['W'] = 1, ['X'] = 1, ['Y'] = 1, ['Z'] = 1,
    ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1, ['g'] = 1, ['h'] = 1, ['i'] = 1, ['j'] = 1,
    ['k'] = 1, ['l'] = 1, ['m'] = 1, ['n'] = 1, ['o'] = 1, ['p'] = 1, ['q'] = 1, ['r'] = 1, ['s'] = 1, ['t'] = 1,
    ['u'] = 1, ['v'] = 1, ['w'] = 1, ['x'] = 1, ['y'] = 1, ['z'] = 1,
    ['-'] = 1, ['_'] = 1, ['.'] = 1, ['~'] = 1,
};
static const char hex[16] = "0123456789ABCDEF";
#if defined(__x86_64__) && defined(__AVX2__)
static size_t escape_url_avx2(const unsigned char *restrict s, size_t query_len, char *restrict d) {
    size_t o = 0;
    size_t i = 0;
    while (i + 32 <= query_len) {
        __m256i chars = _mm256_loadu_si256((__m256i *)(s + i));
        __m256i zero_m1 = _mm256_set1_epi8('0' - 1);
        __m256i ge0 = _mm256_cmpgt_epi8(chars, zero_m1);
        __m256i nine_p1 = _mm256_set1_epi8('9' + 1);
        __m256i le9 = _mm256_cmpgt_epi8(nine_p1, chars);
        __m256i mask_num = _mm256_and_si256(ge0, le9);
        __m256i A_m1 = _mm256_set1_epi8('A' - 1);
        __m256i geA = _mm256_cmpgt_epi8(chars, A_m1);
        __m256i Z_p1 = _mm256_set1_epi8('Z' + 1);
        __m256i leZ = _mm256_cmpgt_epi8(Z_p1, chars);
        __m256i mask_AZ = _mm256_and_si256(geA, leZ);
        __m256i a_m1 = _mm256_set1_epi8('a' - 1);
        __m256i gea = _mm256_cmpgt_epi8(chars, a_m1);
        __m256i z_p1 = _mm256_set1_epi8('z' + 1);
        __m256i lez = _mm256_cmpgt_epi8(z_p1, chars);
        __m256i mask_az = _mm256_and_si256(gea, lez);
        __m256i minus = _mm256_cmpeq_epi8(chars, _mm256_set1_epi8('-'));
        __m256i underscore = _mm256_cmpeq_epi8(chars, _mm256_set1_epi8('_'));
        __m256i dot = _mm256_cmpeq_epi8(chars, _mm256_set1_epi8('.'));
        __m256i tilde = _mm256_cmpeq_epi8(chars, _mm256_set1_epi8('~'));
        __m256i safe = _mm256_or_si256(mask_num, mask_AZ);
        safe = _mm256_or_si256(safe, mask_az);
        safe = _mm256_or_si256(safe, minus);
        safe = _mm256_or_si256(safe, underscore);
        safe = _mm256_or_si256(safe, dot);
        safe = _mm256_or_si256(safe, tilde);
        uint32_t safe_bits = _mm256_movemask_epi8(safe);
        for (int j = 0; j < 32; j++) {
            unsigned char c = s[i + j];
            if (safe_bits & (1u << j)) {
                d[o++] = (char)c;
            } else {
                d[o++] = '%';
                d[o++] = hex[c >> 4];
                d[o++] = hex[c & 15];
            }
        }
        i += 32;
    }
    if (i + 16 <= query_len) {
        __m128i chars = _mm_loadu_si128((__m128i *)(s + i));
        __m128i zero_m1 = _mm_set1_epi8('0' - 1);
        __m128i ge0 = _mm_cmpgt_epi8(chars, zero_m1);
        __m128i nine_p1 = _mm_set1_epi8('9' + 1);
        __m128i le9 = _mm_cmpgt_epi8(nine_p1, chars);
        __m128i mask_num = _mm_and_si128(ge0, le9);
        __m128i A_m1 = _mm_set1_epi8('A' - 1);
        __m128i geA = _mm_cmpgt_epi8(chars, A_m1);
        __m128i Z_p1 = _mm_set1_epi8('Z' + 1);
        __m128i leZ = _mm_cmpgt_epi8(Z_p1, chars);
        __m128i mask_AZ = _mm_and_si128(geA, leZ);
        __m128i a_m1 = _mm_set1_epi8('a' - 1);
        __m128i gea = _mm_cmpgt_epi8(chars, a_m1);
        __m128i z_p1 = _mm_set1_epi8('z' + 1);
        __m128i lez = _mm_cmpgt_epi8(z_p1, chars);
        __m128i mask_az = _mm_and_si128(gea, lez);
        __m128i minus = _mm_cmpeq_epi8(chars, _mm_set1_epi8('-'));
        __m128i underscore = _mm_cmpeq_epi8(chars, _mm_set1_epi8('_'));
        __m128i dot = _mm_cmpeq_epi8(chars, _mm_set1_epi8('.'));
        __m128i tilde = _mm_cmpeq_epi8(chars, _mm_set1_epi8('~'));
        __m128i safe = _mm_or_si128(mask_num, mask_AZ);
        safe = _mm_or_si128(safe, mask_az);
        safe = _mm_or_si128(safe, minus);
        safe = _mm_or_si128(safe, underscore);
        safe = _mm_or_si128(safe, dot);
        safe = _mm_or_si128(safe, tilde);
        uint16_t safe_bits = _mm_movemask_epi8(safe);
        for (int j = 0; j < 16; j++) {
            unsigned char c = s[i + j];
            if (safe_bits & (1u << j)) {
                d[o++] = (char)c;
            } else {
                d[o++] = '%';
                d[o++] = hex[c >> 4];
                d[o++] = hex[c & 15];
            }
        }
        i += 16;
    }
    const unsigned char *end = s + query_len;
    s += i;
    while (s < end) {
        unsigned char c = *s++;
        if (likely(url_safe[c])) {
            d[o++] = (char)c;
        } else {
            d[o++] = '%';
            d[o++] = hex[c >> 4];
            d[o++] = hex[c & 15];
        }
    }
    return o;
}
#endif
#if defined(__x86_64__) && defined(__AVX512F__)
static size_t escape_url_avx512(const unsigned char *s, size_t query_len, char *d) {
    size_t o = 0;
    size_t i = 0;
    while (i + 64 <= query_len) {
        size_t batch = 64;
        __mmask64 full_mask = ~0ULL;
        __m512i chars = _mm512_maskz_loadu_epi8(full_mask, (__m512i *)(s + i));
        __m512i zero_epi8 = _mm512_set1_epi8('0');
        __m512i nine_epi8 = _mm512_set1_epi8('9');
        __mmask64 ge0 = _mm512_cmp_epi8_mask(chars, zero_epi8, _CMP_GE_OQ);
        __mmask64 le9 = _mm512_cmp_epi8_mask(chars, nine_epi8, _CMP_LE_OQ);
        __mmask64 mask_num = ge0 & le9 & full_mask;
        __m512i A_epi8 = _mm512_set1_epi8('A');
        __m512i Z_epi8 = _mm512_set1_epi8('Z');
        __mmask64 geA = _mm512_cmp_epi8_mask(chars, A_epi8, _CMP_GE_OQ);
        __mmask64 leZ = _mm512_cmp_epi8_mask(chars, Z_epi8, _CMP_LE_OQ);
        __mmask64 mask_AZ = geA & leZ & full_mask;
        __m512i a_epi8 = _mm512_set1_epi8('a');
        __m512i z_epi8 = _mm512_set1_epi8('z');
        __mmask64 gea = _mm512_cmp_epi8_mask(chars, a_epi8, _CMP_GE_OQ);
        __mmask64 lez = _mm512_cmp_epi8_mask(chars, z_epi8, _CMP_LE_OQ);
        __mmask64 mask_az = gea & lez & full_mask;
        __mmask64 mask_minus = _mm512_cmp_epi8_mask(chars, _mm512_set1_epi8('-'), _CMP_EQ_OQ) & full_mask;
        __mmask64 mask_underscore = _mm512_cmp_epi8_mask(chars, _mm512_set1_epi8('_'), _CMP_EQ_OQ) & full_mask;
        __mmask64 mask_dot = _mm512_cmp_epi8_mask(chars, _mm512_set1_epi8('.'), _CMP_EQ_OQ) & full_mask;
        __mmask64 mask_tilde = _mm512_cmp_epi8_mask(chars, _mm512_set1_epi8('~'), _CMP_EQ_OQ) & full_mask;
        __mmask64 safe_mask = (mask_num | mask_AZ | mask_az | mask_minus | mask_underscore | mask_dot | mask_tilde) & full_mask;
        for (size_t j = 0; j < batch; ++j) {
            unsigned char c = s[i + j];
            if (safe_mask & (1ULL << j)) {
                d[o++] = (char)c;
            } else {
                d[o++] = '%';
                d[o++] = hex[c >> 4];
                d[o++] = hex[c & 15];
            }
        }
        i += 64;
    }
    const unsigned char *end = s + query_len;
    s += i;
    while (s < end) {
        unsigned char c = *s++;
        if (likely(url_safe[c])) {
            d[o++] = (char)c;
        } else {
            d[o++] = '%';
            d[o++] = hex[c >> 4];
            d[o++] = hex[c & 15];
        }
    }
    return o;
}
#endif
#if defined(__arm__) || defined(__aarch64__)
static size_t escape_url_neon(const unsigned char *s, size_t query_len, char *d) {
    size_t o = 0;
    size_t i = 0;
    while (i + 16 <= query_len) {
        uint8x16_t chars = vld1q_u8(s + i);
        uint8x16_t zero = vdupq_n_u8('0');
        uint8x16_t ge0 = vcgeq_u8(chars, zero);
        uint8x16_t nine = vdupq_n_u8('9');
        uint8x16_t le9 = vcleq_u8(chars, nine);
        uint8x16_t mask_num = vandq_u8(ge0, le9);
        uint8x16_t A = vdupq_n_u8('A');
        uint8x16_t geA = vcgeq_u8(chars, A);
        uint8x16_t Z = vdupq_n_u8('Z');
        uint8x16_t leZ = vcleq_u8(chars, Z);
        uint8x16_t mask_AZ = vandq_u8(geA, leZ);
        uint8x16_t a = vdupq_n_u8('a');
        uint8x16_t gea = vcgeq_u8(chars, a);
        uint8x16_t z = vdupq_n_u8('z');
        uint8x16_t lez = vcleq_u8(chars, z);
        uint8x16_t mask_az = vandq_u8(gea, lez);
        uint8x16_t minus = vceqq_u8(chars, vdupq_n_u8('-'));
        uint8x16_t underscore = vceqq_u8(chars, vdupq_n_u8('_'));
        uint8x16_t dot = vceqq_u8(chars, vdupq_n_u8('.'));
        uint8x16_t tilde = vceqq_u8(chars, vdupq_n_u8('~'));
        uint8x16_t safe = vorrq_u8(mask_num, mask_AZ);
        safe = vorrq_u8(safe, mask_az);
        safe = vorrq_u8(safe, minus);
        safe = vorrq_u8(safe, underscore);
        safe = vorrq_u8(safe, dot);
        safe = vorrq_u8(safe, tilde);
        uint16_t mask = 0;
        uint8x8_t low = vget_low_u8(safe);
        uint8x8_t high = vget_high_u8(safe);
        for (int k = 0; k < 8; k++) {
            if (vget_lane_u8(low, k) == 0xff) mask |= (1 << k);
            if (vget_lane_u8(high, k) == 0xff) mask |= (1 << (k + 8));
        }
        for (int j = 0; j < 16; j++) {
            unsigned char c = s[i + j];
            if (mask & (1 << j)) {
                d[o++] = (char)c;
            } else {
                d[o++] = '%';
                d[o++] = hex[c >> 4];
                d[o++] = hex[c & 15];
            }
        }
        i += 16;
    }
    const unsigned char *end = s + query_len;
    s += i;
    while (s < end) {
        unsigned char c = *s++;
        if (likely(url_safe[c])) {
            d[o++] = (char)c;
        } else {
            d[o++] = '%';
            d[o++] = hex[c >> 4];
            d[o++] = hex[c & 15];
        }
    }
    return o;
}
#endif
#if defined(__powerpc64__)
static size_t escape_url_vsx(const unsigned char *s, size_t query_len, char *d) {
    size_t o = 0;
    size_t i = 0;
    typedef vector unsigned char vec_u8;
    while (i + 16 <= query_len) {
        vec_u8 chars = vec_vsx_ld(0, s + i);
        vec_u8 zero_m1 = vec_splat_u8('0' - 1);
        vec_u8 ge0 = vec_cmpgt(chars, zero_m1);
        vec_u8 nine_p1 = vec_splat_u8('9' + 1);
        vec_u8 le9 = vec_cmpgt(nine_p1, chars);
        vec_u8 mask_num = vec_and(ge0, le9);
        vec_u8 A_m1 = vec_splat_u8('A' - 1);
        vec_u8 geA = vec_cmpgt(chars, A_m1);
        vec_u8 Z_p1 = vec_splat_u8('Z' + 1);
        vec_u8 leZ = vec_cmpgt(Z_p1, chars);
        vec_u8 mask_AZ = vec_and(geA, leZ);
        vec_u8 a_m1 = vec_splat_u8('a' - 1);
        vec_u8 gea = vec_cmpgt(chars, a_m1);
        vec_u8 z_p1 = vec_splat_u8('z' + 1);
        vec_u8 lez = vec_cmpgt(z_p1, chars);
        vec_u8 mask_az = vec_and(gea, lez);
        vec_u8 minus = vec_cmpeq(chars, vec_splat_u8('-'));
        vec_u8 underscore = vec_cmpeq(chars, vec_splat_u8('_'));
        vec_u8 dot = vec_cmpeq(chars, vec_splat_u8('.'));
        vec_u8 tilde = vec_cmpeq(chars, vec_splat_u8('~'));
        vec_u8 safe = vec_or(mask_num, mask_AZ);
        safe = vec_or(safe, mask_az);
        safe = vec_or(safe, minus);
        safe = vec_or(safe, underscore);
        safe = vec_or(safe, dot);
        safe = vec_or(safe, tilde);
        unsigned char safe_bytes[16];
        vec_st(safe, 0, safe_bytes);
        for (int j = 0; j < 16; j++) {
            unsigned char c = s[i + j];
            if (safe_bytes[j] == 0xff) {
                d[o++] = (char)c;
            } else {
                d[o++] = '%';
                d[o++] = hex[c >> 4];
                d[o++] = hex[c & 15];
            }
        }
        i += 16;
    }
    const unsigned char *end = s + query_len;
    s += i;
    while (s < end) {
        unsigned char c = *s++;
        if (likely(url_safe[c])) {
            d[o++] = (char)c;
        } else {
            d[o++] = '%';
            d[o++] = hex[c >> 4];
            d[o++] = hex[c & 15];
        }
    }
    return o;
}
#endif
#if defined(__riscv) && defined(__riscv_vector)
static size_t escape_url_rvv(const unsigned char *s, size_t query_len, char *d) {
    size_t o = 0;
    size_t i = 0;
    char safe_arr[4096];
    while (i < query_len) {
        size_t vl = __riscv_vsetvl_e8m1(query_len - i);
        vuint8m1_t chars = __riscv_vle8_v_u8m1(s + i, vl);
        vbool8_t ge0 = __riscv_vmsgeu_vx_u8m1_b8(chars, '0', vl);
        vbool8_t le9 = __riscv_vmsleu_vx_u8m1_b8(chars, '9', vl);
        vbool8_t mask_num = __riscv_vmand_mm_b8(ge0, le9, vl);
        vbool8_t geA = __riscv_vmsgeu_vx_u8m1_b8(chars, 'A', vl);
        vbool8_t leZ = __riscv_vmsleu_vx_u8m1_b8(chars, 'Z', vl);
        vbool8_t mask_AZ = __riscv_vmand_mm_b8(geA, leZ, vl);
        vbool8_t gea = __riscv_vmsgeu_vx_u8m1_b8(chars, 'a', vl);
        vbool8_t lez = __riscv_vmsleu_vx_u8m1_b8(chars, 'z', vl);
        vbool8_t mask_az = __riscv_vmand_mm_b8(gea, lez, vl);
        vbool8_t minus = __riscv_vmseq_vx_u8m1_b8(chars, '-', vl);
        vbool8_t underscore = __riscv_vmseq_vx_u8m1_b8(chars, '_', vl);
        vbool8_t dot = __riscv_vmseq_vx_u8m1_b8(chars, '.', vl);
        vbool8_t tilde = __riscv_vmseq_vx_u8m1_b8(chars, '~', vl);
        vbool8_t safe = __riscv_vmor_mm_b8(mask_num, mask_AZ, vl);
        safe = __riscv_vmor_mm_b8(safe, mask_az, vl);
        safe = __riscv_vmor_mm_b8(safe, minus, vl);
        safe = __riscv_vmor_mm_b8(safe, underscore, vl);
        safe = __riscv_vmor_mm_b8(safe, dot, vl);
        safe = __riscv_vmor_mm_b8(safe, tilde, vl);
        vuint8m1_t ones = __riscv_vmv_v_x_u8m1(1, vl);
        vuint8m1_t zeros = __riscv_vmv_v_x_u8m1(0, vl);
        vuint8m1_t safe_u8 = __riscv_vmerge_vvm_u8m1(safe, zeros, ones, vl);
        __riscv_vse8_v_u8m1((uint8_t *)safe_arr, safe_u8, vl);
        for (size_t j = 0; j < vl; j++) {
            unsigned char c = s[i + j];
            if (safe_arr[j]) {
                d[o++] = (char)c;
            } else {
                d[o++] = '%';
                d[o++] = hex[c >> 4];
                d[o++] = hex[c & 15];
            }
        }
        i += vl;
    }
    return o;
}
#endif
#if defined(__aarch64__) && defined(__ARM_FEATURE_SVE)
static size_t escape_url_sve(const unsigned char *s, size_t query_len, char *d) {
    size_t o = 0;
    size_t i = 0;
    char safe_arr[4096];
    while (i < query_len) {
        svbool_t pg = svwhilelt_b8(i, query_len);
        svuint8_t chars = svld1_u8(pg, s + i);
        svbool_t ge0 = svcmpge_u8(pg, chars, '0');
        svbool_t le9 = svcmple_u8(pg, chars, '9');
        svbool_t mask_num = svand_b_z(pg, ge0, le9);
        svbool_t geA = svcmpge_u8(pg, chars, 'A');
        svbool_t leZ = svcmple_u8(pg, chars, 'Z');
        svbool_t mask_AZ = svand_b_z(pg, geA, leZ);
        svbool_t gea = svcmpge_u8(pg, chars, 'a');
        svbool_t lez = svcmple_u8(pg, chars, 'z');
        svbool_t mask_az = svand_b_z(pg, gea, lez);
        svbool_t minus = svcmpeq_u8(pg, chars, '-');
        svbool_t underscore = svcmpeq_u8(pg, chars, '_');
        svbool_t dot = svcmpeq_u8(pg, chars, '.');
        svbool_t tilde = svcmpeq_u8(pg, chars, '~');
        svbool_t safe = svorr_b_z(pg, mask_num, mask_AZ);
        safe = svorr_b_z(pg, safe, mask_az);
        safe = svorr_b_z(pg, safe, minus);
        safe = svorr_b_z(pg, safe, underscore);
        safe = svorr_b_z(pg, safe, dot);
        safe = svorr_b_z(pg, safe, tilde);
        size_t vl = svcntb();
        svst1_u8(pg, (uint8_t *)safe_arr, svsel_u8(safe, svdup_u8(1), svdup_u8(0)));
        for (size_t j = 0; j < vl; j++) {
            unsigned char c = s[i + j];
            if (safe_arr[j]) {
                d[o++] = (char)c;
            } else {
                d[o++] = '%';
                d[o++] = hex[c >> 4];
                d[o++] = hex[c & 15];
            }
        }
        i += vl;
    }
    return o;
}
#endif
struct buf_t {
    char *data;
    size_t len;
};
#ifdef _WIN32
typedef BOOL (WINAPI *LPFN_CONNECTEX)(SOCKET, const struct sockaddr *, int, PVOID, DWORD, LPDWORD, LPOVERLAPPED);
#endif
int main(int argc, char **argv) {
    memset(hex_val, -1, sizeof(hex_val));
    for (char c = '0'; c <= '9'; c++) hex_val[(unsigned char)c] = c - '0';
    for (char c = 'A'; c <= 'F'; c++) {
        hex_val[(unsigned char)c] = 10 + c - 'A';
        hex_val[(unsigned char)(c + 32)] = 10 + c - 'A';
    }
#if defined(__x86_64__) || defined(_M_X64)
#if defined(__AVX512F__)
    if (has_avx512()) memmem_opt = memmem_avx512;
    else
#endif
#if defined(__AVX2__)
    if (has_avx2()) memmem_opt = memmem_avx2;
    else
#endif
        memmem_opt = memmem_scalar;
#else
    memmem_opt = memmem_scalar;
#endif
#ifdef _WIN32
    WSADATA wsa;
    if (unlikely(WSAStartup(MAKEWORD(2,2), &wsa) != 0)) {
        WRITE_FD(STDERR_FD, "WSAStartup failed\n", 18);
        return 1;
    }
    fprintf(stderr, "debug: WSAStartup ok\n");
#endif
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    SSL_load_error_strings();
    long long t_start = 0;
    long long t_init_end = 0;
    long long t_escape_start = 0;
    long long t_escape_end = 0;
    long long t_url_start = 0;
    long long t_url_end = 0;
    long long t_setup_start = 0;
    long long t_setup_end = 0;
    long long t_perform_start = 0;
    long long t_perform_end = 0;
    int verbose = 0;
    const char *search_term = NULL;
    if (argc == 2) {
        search_term = argv[1];
    } else if (argc == 3 && argv[1][0] == '-' && argv[1][1] == 'v' && argv[1][2] == 0) {
        verbose = 1;
        search_term = argv[2];
    } else {
        const char *u1 = "Usage: ";
        WRITE_FD(STDERR_FD, u1, 7);
        WRITE_FD(STDERR_FD, argv[0], (unsigned int)strlen(argv[0]));
        WRITE_FD(STDERR_FD, " [-v] <search_term>\n", 20);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    if (verbose) t_start = current_time_ns();
    if (verbose) t_init_end = current_time_ns();
    if (verbose) t_escape_start = current_time_ns();
    size_t query_len = strlen(search_term);
    if (query_len > MAX_QUERY_LEN) {
        WRITE_FD(STDERR_FD, "Search term too long\n", 21);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    char escaped[MAX_ESCAPED_SIZE];
    const unsigned char *restrict s = (const unsigned char *)search_term;
    char *restrict d = escaped;
    size_t escaped_len = 0;
    size_t o = 0;
#if defined(__x86_64__) || defined(_M_X64)
#if defined(__AVX512F__)
    if (has_avx512() && query_len >= 64) {
        o = escape_url_avx512(s, query_len, d);
    } else
#endif
#if defined(__AVX2__)
    if (has_avx2() && query_len >= 32) {
        o = escape_url_avx2(s, query_len, d);
    } else
#endif
    {
        const unsigned char *end = s + query_len;
        while (s < end) {
            unsigned char c = *s++;
            if (likely(url_safe[c])) {
                d[o++] = (char)c;
            } else {
                d[o++] = '%';
                d[o++] = hex[c >> 4];
                d[o++] = hex[c & 15];
            }
        }
    }
#elif defined(__aarch64__) && defined(__ARM_FEATURE_SVE)
    o = escape_url_sve(s, query_len, d);
#elif defined(__arm__) || defined(__aarch64__)
    o = escape_url_neon(s, query_len, d);
#elif defined(__powerpc64__)
    o = escape_url_vsx(s, query_len, d);
#elif defined(__riscv) && defined(__riscv_vector)
    o = escape_url_rvv(s, query_len, d);
#else
    const unsigned char *end = s + query_len;
    while (s < end) {
        unsigned char c = *s++;
        if (likely(url_safe[c])) {
            d[o++] = (char)c;
        } else {
            d[o++] = '%';
            d[o++] = hex[c >> 4];
            d[o++] = hex[c & 15];
        }
    }
#endif
    d[o] = 0;
    escaped_len = o;
    if (unlikely(escaped_len == 0 && query_len > 0)) {
        WRITE_FD(STDERR_FD, "URL escape failed\n", 18);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    if (verbose) t_escape_end = current_time_ns();
    if (verbose) t_url_start = current_time_ns();
    static const char prefix[] = "GET /q.php?q=";
    size_t pre_len = sizeof(prefix) - 1;
    static const char suffix[] = " HTTP/1.1\r\nHost: apibay.org\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36\r\nAccept: */*\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";
    size_t suf_len = sizeof(suffix) - 1;
    size_t request_len = pre_len + escaped_len + suf_len;
    if (verbose) t_url_end = current_time_ns();
    if (verbose) t_setup_start = current_time_ns();
    if (verbose) t_setup_end = current_time_ns();
    if (verbose) t_perform_start = current_time_ns();
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res;
    int gai_err = getaddrinfo("apibay.org", "443", &hints, &res);
    if (gai_err != 0) {
        WRITE_FD(STDERR_FD, "getaddrinfo failed\n", 19);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    fprintf(stderr, "debug: getaddrinfo ok\n");
    sock_t sock = INVALID_SOCKET;
    struct addrinfo *rp;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fprintf(stderr, "debug: trying addr family %d\n", rp->ai_family);
        if (rp->ai_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &((struct sockaddr_in*)rp->ai_addr)->sin_addr, ip, sizeof(ip)) == NULL) {
                fprintf(stderr, "debug: inet_ntop failed %d\n", errno);
            } else {
                fprintf(stderr, "debug: Resolved IPv4: %s\n", ip);
            }
        } else if (rp->ai_family == AF_INET6) {
            char ip[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &((struct sockaddr_in6*)rp->ai_addr)->sin6_addr, ip, sizeof(ip)) == NULL) {
                fprintf(stderr, "debug: inet_ntop failed %d\n", errno);
            } else {
                fprintf(stderr, "debug: Resolved IPv6: %s\n", ip);
            }
        }
#ifdef _WIN32
        sock = WSASocket(rp->ai_family, rp->ai_socktype, rp->ai_protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
#endif
        if (sock == INVALID_SOCKET) {
#ifdef _WIN32
            fprintf(stderr, "debug: socket failed %d\n", WSAGetLastError());
#else
            fprintf(stderr, "debug: socket failed %d\n", errno);
#endif
            continue;
        }
        fprintf(stderr, "debug: socket created\n");
        int opt = 1;
        if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const void *)&opt, sizeof(opt)) == SOCKET_ERROR) {
#ifdef _WIN32
            fprintf(stderr, "debug: setsockopt TCP_NODELAY failed %d\n", WSAGetLastError());
#else
            fprintf(stderr, "debug: setsockopt TCP_NODELAY failed %d\n", errno);
#endif
        }
        int sndbuf = 65536;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const void *)&sndbuf, sizeof(sndbuf)) == SOCKET_ERROR) {
#ifdef _WIN32
            fprintf(stderr, "debug: setsockopt SO_SNDBUF failed %d\n", WSAGetLastError());
#else
            fprintf(stderr, "debug: setsockopt SO_SNDBUF failed %d\n", errno);
#endif
        }
        int bufsize = 2097152;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const void *)&bufsize, sizeof(bufsize)) == SOCKET_ERROR) {
#ifdef _WIN32
            fprintf(stderr, "debug: setsockopt SO_RCVBUF failed %d\n", WSAGetLastError());
#else
            fprintf(stderr, "debug: setsockopt SO_RCVBUF failed %d\n", errno);
#endif
        }
#ifndef _WIN32
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1) {
            fprintf(stderr, "debug: fcntl F_GETFL failed %d\n", errno);
        }
        if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
            fprintf(stderr, "debug: fcntl O_NONBLOCK failed %d\n", errno);
        }
#endif
        bool connected = false;
#ifdef _WIN32
        LPFN_CONNECTEX ConnectExPtr = NULL;
        GUID guidConnectEx = WSAID_CONNECTEX;
        DWORD ioctl_bytes = 0;
        int ioctl_ret = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidConnectEx, sizeof(guidConnectEx), &ConnectExPtr, sizeof(ConnectExPtr), &ioctl_bytes, NULL, NULL);
        if (ioctl_ret == 0 && ConnectExPtr != NULL) {
            fprintf(stderr, "debug: Got ConnectEx\n");
            struct sockaddr_storage local_addr = {0};
            int local_addr_len = rp->ai_addrlen;
            memset(&local_addr, 0, local_addr_len);
            ((struct sockaddr*)&local_addr)->sa_family = rp->ai_family;
            if (bind(sock, (struct sockaddr *)&local_addr, local_addr_len) == SOCKET_ERROR) {
                fprintf(stderr, "debug: bind failed %d\n", WSAGetLastError());
                closesocket(sock);
                continue;
            }
            fprintf(stderr, "debug: bind ok\n");
            WSAEVENT connect_event = WSACreateEvent();
            if (connect_event == WSA_INVALID_EVENT) {
                fprintf(stderr, "debug: WSACreateEvent failed\n");
                closesocket(sock);
                continue;
            }
            WSAOVERLAPPED ov = {0};
            ov.hEvent = connect_event;
            BOOL c_ret = ConnectExPtr(sock, rp->ai_addr, (int)rp->ai_addrlen, NULL, 0, NULL, &ov);
            if (c_ret == TRUE) {
                connected = true;
                setsockopt(sock, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
                fprintf(stderr, "debug: ConnectEx immediate success\n");
            } else {
                int err = WSAGetLastError();
                fprintf(stderr, "debug: ConnectEx returned FALSE with error %d\n", err);
                if (err == ERROR_IO_PENDING) {
                    fprintf(stderr, "debug: ConnectEx pending\n");
                    DWORD wait = WSAWaitForMultipleEvents(1, &connect_event, TRUE, 5000, FALSE);
                    fprintf(stderr, "debug: WSAWaitForMultipleEvents returned %lu\n", (unsigned long)wait);
                    if (wait == WSA_WAIT_EVENT_0) {
                        DWORD bytes_transferred = 0;
                        DWORD flags = 0;
                        BOOL result = WSAGetOverlappedResult(sock, &ov, &bytes_transferred, FALSE, &flags);
                        err = WSAGetLastError();
                        fprintf(stderr, "debug: WSAGetOverlappedResult returned %d with error %d\n", result, err);
                        if (result) {
                            connected = true;
                            setsockopt(sock, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
                            fprintf(stderr, "debug: GetOverlapped success\n");
                        } else {
                            fprintf(stderr, "debug: GetOverlapped failed %d\n", err);
                        }
                    } else {
                        fprintf(stderr, "debug: wait failed %lu\n", (unsigned long)wait);
                    }
                } else {
                    fprintf(stderr, "debug: ConnectEx failed %d\n", err);
                }
            }
            WSACloseEvent(connect_event);
        } else {
            fprintf(stderr, "debug: WSAIoctl for ConnectEx failed %d\n", WSAGetLastError());
            // Fallback to regular connect
            u_long non_block = 1;
            if (ioctlsocket(sock, FIONBIO, &non_block) == SOCKET_ERROR) {
                fprintf(stderr, "debug: ioctlsocket non_block failed %d\n", WSAGetLastError());
                closesocket(sock);
                continue;
            }
            fprintf(stderr, "debug: fallback to connect\n");
            int c_ret = connect(sock, rp->ai_addr, (int)rp->ai_addrlen);
            if (c_ret == 0) {
                connected = true;
                fprintf(stderr, "debug: connect immediate success\n");
            } else {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) {
                    fprintf(stderr, "debug: connect would block\n");
                    WSAEVENT connect_event = WSACreateEvent();
                    if (connect_event == WSA_INVALID_EVENT) {
                        fprintf(stderr, "debug: WSACreateEvent failed\n");
                        closesocket(sock);
                        continue;
                    }
                    if (WSAEventSelect(sock, connect_event, FD_CONNECT) == SOCKET_ERROR) {
                        fprintf(stderr, "debug: WSAEventSelect failed %d\n", WSAGetLastError());
                        WSACloseEvent(connect_event);
                        closesocket(sock);
                        continue;
                    }
                    DWORD wait = WSAWaitForMultipleEvents(1, &connect_event, TRUE, 5000, FALSE);
                    if (wait == WSA_WAIT_EVENT_0) {
                        WSANETWORKEVENTS events;
                        if (WSAEnumNetworkEvents(sock, connect_event, &events) == 0) {
                            if (events.lNetworkEvents & FD_CONNECT) {
                                if (events.iErrorCode[FD_CONNECT_BIT] == 0) {
                                    connected = true;
                                    fprintf(stderr, "debug: connect success via event\n");
                                } else {
                                    fprintf(stderr, "debug: connect error %d\n", events.iErrorCode[FD_CONNECT_BIT]);
                                }
                            } else {
                                fprintf(stderr, "debug: no FD_CONNECT\n");
                            }
                        } else {
                            fprintf(stderr, "debug: WSAEnumNetworkEvents failed %d\n", WSAGetLastError());
                        }
                    } else {
                        fprintf(stderr, "debug: wait failed %lu\n", (unsigned long)wait);
                    }
                    WSAEventSelect(sock, NULL, 0); // Clear event association
                    WSACloseEvent(connect_event);
                } else {
                    fprintf(stderr, "debug: connect failed %d\n", err);
                }
            }
            if (connected) {
                u_long block = 0;
                if (ioctlsocket(sock, FIONBIO, &block) == SOCKET_ERROR) {
                    fprintf(stderr, "debug: ioctlsocket block failed %d\n", WSAGetLastError());
                }
            }
        }
#else
        int c_ret = connect(sock, rp->ai_addr, rp->ai_addrlen);
        if (c_ret == 0) {
            connected = true;
            fprintf(stderr, "debug: connect success\n");
        } else if (errno == EINPROGRESS) {
            fprintf(stderr, "debug: connect in progress\n");
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);
            struct timeval tv = {5, 0};
            int sel_ret = select((int)sock + 1, NULL, &write_fds, NULL, &tv);
            if (sel_ret > 0 && FD_ISSET(sock, &write_fds)) {
                int err_val;
                socklen_t err_len = sizeof(err_val);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&err_val, &err_len) == 0 && err_val == 0) {
                    connected = true;
                    fprintf(stderr, "debug: connect success via select\n");
                } else {
                    fprintf(stderr, "debug: connect error %d\n", err_val);
                }
            } else {
                fprintf(stderr, "debug: select failed %d\n", sel_ret);
            }
        } else {
            fprintf(stderr, "debug: connect failed %d\n", errno);
        }
#endif
        if (connected) {
#ifdef _WIN32
            u_long block = 0;
            if (ioctlsocket(sock, FIONBIO, &block) == SOCKET_ERROR) {
                fprintf(stderr, "debug: ioctlsocket to blocking failed %d\n", WSAGetLastError());
            }
#else
            int flags = fcntl(sock, F_GETFL, 0);
            if (flags != -1) {
                fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
            }
#endif
            break;
        }
        close_sock(sock);
        sock = INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "debug: all connects failed\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == NULL) {
        fprintf(stderr, "debug: SSL_CTX_new failed\n");
        close_sock(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    SSL *ssl = SSL_new(ctx);
    if (ssl == NULL) {
        fprintf(stderr, "debug: SSL_new failed\n");
        SSL_CTX_free(ctx);
        close_sock(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    SSL_set_fd(ssl, (int)sock);
    SSL_set_tlsext_host_name(ssl, "apibay.org");
    if (SSL_connect(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        fprintf(stderr, "debug: SSL_connect failed\n");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close_sock(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
#ifdef _WIN32
    DWORD timeout = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv = {5, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const void*)&tv, sizeof(tv));
#endif
    char request[4096];
    if (request_len >= sizeof(request)) {
        fprintf(stderr, "debug: request too long\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close_sock(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    memcpy(request, prefix, pre_len);
    memcpy(request + pre_len, escaped, escaped_len);
    memcpy(request + pre_len + escaped_len, suffix, suf_len);
    ssize_t sent = 0;
    bool send_success = true;
    while (sent < (ssize_t)request_len) {
        ssize_t this_sent = SSL_write(ssl, request + sent, request_len - sent);
        if (this_sent <= 0) {
            int err = SSL_get_error(ssl, this_sent);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            send_success = false;
            fprintf(stderr, "debug: SSL_write failed %d\n", err);
            break;
        }
        sent += this_sent;
    }
    if (!send_success) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close_sock(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    char *response_buffer = malloc(INITIAL_BUFFER_SIZE + 1);
    if (response_buffer == NULL) {
        fprintf(stderr, "debug: malloc failed\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close_sock(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    size_t capacity = INITIAL_BUFFER_SIZE;
    size_t received = 0;
    while (true) {
        if (received == capacity) {
            size_t new_capacity = capacity * 2;
            char *new_buffer = realloc(response_buffer, new_capacity + 1);
            if (new_buffer == NULL) {
                fprintf(stderr, "debug: realloc failed\n");
                break;
            }
            response_buffer = new_buffer;
            capacity = new_capacity;
        }
        ssize_t r = SSL_read(ssl, response_buffer + received, capacity - received);
        if (r <= 0) {
            int err = SSL_get_error(ssl, r);
            if (err == SSL_ERROR_ZERO_RETURN) break;
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
            fprintf(stderr, "debug: SSL_read failed %d\n", err);
            break;
        }
        received += (size_t)r;
    }
    if (verbose) t_perform_end = current_time_ns();
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close_sock(sock);
    if (received > 0) {
        response_buffer[received] = 0;
        char *body = NULL;
        size_t body_len = 0;
        bool is_chunked = false;
        bool is_200 = false;
        if (received > 4) {
            char *first_nl = (char *)memmem_opt(response_buffer, received, "\r\n", 2);
            if (first_nl) {
                if (strncmp(response_buffer, "HTTP/1.", 7) == 0 && strncmp(response_buffer + 9, "200", 3) == 0) {
                    is_200 = true;
                }
                char *h_start = first_nl + 2;
                char *h_end = (char *)memmem_opt(h_start, received - (h_start - response_buffer), "\r\n\r\n", 4);
                if (h_end) {
                    body = h_end + 4;
                    body_len = response_buffer + received - body;
                    char *line = h_start;
                    while (line < h_end + 2) {
                        char *next_nl = (char *)memmem_opt(line, (h_end + 2) - line, "\r\n", 2);
                        if (next_nl == NULL || next_nl >= h_end + 2) break;
                        size_t line_len = next_nl - line;
                        if (line_len >= 18 && simd_strnicmp(line, "Transfer-Encoding:", 18) == 0) {
                            char *val = line + 18;
                            while (val < next_nl && (*val == ' ' || *val == '\t')) val++;
                            size_t vlen = next_nl - val;
                            if (vlen >= 7 && simd_strnicmp(val, "chunked", 7) == 0) {
                                is_chunked = true;
                            }
                        }
                        line = next_nl + 2;
                    }
                }
            }
        }
        if (!is_200) {
            WRITE_FD(STDERR_FD, "HTTP error\n", 11);
            free(response_buffer);
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }
        if (body) {
            if (!is_chunked) {
                WRITE_FD(STDOUT_FD, body, (unsigned int)body_len);
                if (body_len > 0 && body[body_len - 1] != '\n') {
                    WRITE_FD(STDOUT_FD, "\n", 1);
                }
            } else {
                char *chunk_ptr = body;
                while (chunk_ptr < response_buffer + received) {
                    __builtin_prefetch(chunk_ptr + 64);
                    char *eol = memchr(chunk_ptr, '\r', response_buffer + received - chunk_ptr);
                    if (!eol || *(eol + 1) != '\n') break;
                    size_t line_len = eol - chunk_ptr;
                    size_t chunk_size = parse_hex(chunk_ptr, line_len);
                    chunk_ptr = eol + 2;
                    if (chunk_size == 0) break;
                    if (chunk_ptr + chunk_size + 2 > response_buffer + received) break;
                    if (memcmp(chunk_ptr + chunk_size, "\r\n", 2) != 0) break;
                    WRITE_FD(STDOUT_FD, chunk_ptr, chunk_size);
                    chunk_ptr += chunk_size + 2;
                }
                if (chunk_ptr > body && *(chunk_ptr - 3) != '\n') {
                    WRITE_FD(STDOUT_FD, "\n", 1);
                }
            }
        } else {
            WRITE_FD(STDERR_FD, "No headers found\n", 17);
        }
    } else {
        WRITE_FD(STDERR_FD, "No data received\n", 17);
    }
    free(response_buffer);
    if (verbose) {
        long long d_init = t_init_end - t_start;
        long long d_escape = t_escape_end - t_escape_start;
        long long d_url = t_url_end - t_url_start;
        long long d_setup = t_setup_end - t_setup_start;
        long long d_perform = t_perform_end - t_perform_start;
        long long total = t_perform_end - t_start;
        printf("Initialization: %lld ns\n", d_init);
        printf("URL Escaping: %lld ns\n", d_escape);
        printf("URL Building: %lld ns\n", d_url);
        printf("Curl Setup: %lld ns\n", d_setup);
        printf("HTTP Request: %lld ns\n", d_perform);
        printf("Total: %lld ns\n", total);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
