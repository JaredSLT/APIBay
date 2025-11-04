// main.c
// Standalone minimal HTTP/1.1 client (no libcurl) for TPBSearch - OPTIMIZED FOR SPEED
// Build (MSYS2/MinGW/clang/gcc): clang -O3 -march=native -std=gnu23 main.c -o TPBSearch
// On Windows the Ws2_32 library is used via Winsock calls (WSAStartup/WSACleanup).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>             // _write
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <intrin.h>         // For __cpuidex
#include <mswsock.h>        // For ConnectEx
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET sock_t;
#define close_sock(s) closesocket(s)
#define WRITE_FD _write
#define STDOUT_FD 1
#define STDERR_FD 2
static LARGE_INTEGER g_freq;
#ifndef TCP_FASTOPEN
#define TCP_FASTOPEN 15
#endif
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define close_sock(s) close(s)
#define WRITE_FD write
#include <unistd.h>
#define STDOUT_FD STDOUT_FILENO
#define STDERR_FD STDERR_FILENO
#endif

static void *memmem_scalar(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (needlelen == 0) {
        return (void *)haystack;
    }
    if (haystacklen < needlelen) {
        return NULL;
    }
    const char *h = (const char *)haystack;
    const char *n = (const char *)needle;
    for (size_t i = 0; i <= haystacklen - needlelen; ++i) {
        if (memcmp(h + i, n, needlelen) == 0) {
            return (void *)(h + i);
        }
    }
    return NULL;
}

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
static inline uint64_t rdtsc(void) {
    unsigned int low, high;
    __asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t)high << 32) | low;
}
static double g_tsc_freq = 0.0;
static uint64_t g_tsc_base = 0;
static long long g_time_base = 0;
#endif

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#if defined(__aarch64__) || defined(__arm__)
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif
#endif

#if defined(__powerpc__) || defined(__powerpc64__)
#include <altivec.h>
#undef vector
#undef bool
#undef pixel
#endif

#if defined(__riscv)
#if defined(__riscv_vector)
#include <riscv_vector.h>
#endif
#endif

#ifdef __linux__
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#define RESPONSE_BUFFER_SIZE 131072UL
#define MAX_QUERY_LEN 1024
#define MAX_ESCAPED_SIZE ((MAX_QUERY_LEN * 3) + 1)
#define MAX_REQ_SIZE 2048

#ifndef likely
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif

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
    int info[4];
    cpuid(info, 0);
    return (info[1] == 0x756e6547 && info[3] == 0x49656e69 && info[2] == 0x6c65746e);
}

static inline int is_amd(void) {
    int info[4];
    cpuid(info, 0);
    return (info[1] == 0x68747541 && info[3] == 0x69746e65 && info[2] == 0x444d4163);
}

static inline long long current_time_ns(void) {
#if defined(__x86_64__) || defined(__i386__)
    static bool has_invariant_tsc = false;
    if (unlikely(g_tsc_freq == 0.0)) {
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
                            if (model >= 0x3D) { // Broadwell and later
                                crystal_hz = 24000000ULL;
                            } else if (model >= 0x0F) { // Core 2 and later pre-Broadwell
                                crystal_hz = 25000000ULL;
                            } else {
                                crystal_hz = 25000000ULL; // Default for older
                            }
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
                if (ghz_pos) {
                    char *freq_start = ghz_pos - 1;
                    while (freq_start > brand_str && (isdigit(*freq_start) || *freq_start == '.')) {
                        freq_start--;
                    }
                    freq_start++;
                    double freq_ghz = atof(freq_start);
                    g_tsc_freq = freq_ghz * 1000000000.0;
                }
            }
        }
        if (g_tsc_freq == 0.0) {
            // Fallback to calibration if invariant but no freq, or no invariant
            uint64_t tsc_start = rdtsc();
#ifdef _WIN32
            LARGE_INTEGER cnt_start;
            QueryPerformanceCounter(&cnt_start);
            Sleep(100);
            LARGE_INTEGER cnt_end;
            QueryPerformanceCounter(&cnt_end);
            long long calib_start_ns = (cnt_start.QuadPart * 1000000000LL) / g_freq.QuadPart;
            long long calib_end_ns = (cnt_end.QuadPart * 1000000000LL) / g_freq.QuadPart;
#else
            struct timespec ts_start, ts_end;
            clock_gettime(CLOCK_MONOTONIC, &ts_start);
            usleep(100000);
            clock_gettime(CLOCK_MONOTONIC, &ts_end);
            long long calib_start_ns = (ts_start.tv_sec * 1000000000LL) + ts_start.tv_nsec;
            long long calib_end_ns = (ts_end.tv_sec * 1000000000LL) + ts_end.tv_nsec;
#endif
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
#ifdef _WIN32
        LARGE_INTEGER cnt;
        QueryPerformanceCounter(&cnt);
        return (cnt.QuadPart * 1000000000LL) / g_freq.QuadPart;
#else
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (ts.tv_sec * 1000000000LL) + ts.tv_nsec;
#endif
    }
#else
#ifdef _WIN32
    LARGE_INTEGER cnt;
    QueryPerformanceCounter(&cnt);
    return (cnt.QuadPart * 1000000000LL) / g_freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000000LL) + ts.tv_nsec;
#endif
#endif
}

static inline int has_avx2() {
    int info[4];
#ifdef __GNUC__
    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "0" (1), "2" (0)
    );
#else
    __cpuidex(info, 1, 0);
#endif
    if ((info[2] & (1 << 27)) == 0 || (info[2] & (1 << 28)) == 0) return 0;
    unsigned long long xcr;
#ifdef __GNUC__
    xcr = __builtin_ia32_xgetbv(0);
#else
    xcr = _xgetbv(0);
#endif
    if ((xcr & 6) != 6) return 0;
#ifdef __GNUC__
    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "0" (7), "2" (0)
    );
#else
    __cpuidex(info, 7, 0);
#endif
    return (info[1] & (1 << 5)) != 0;
}

static inline int has_avx512() {
    int info[4];
#ifdef __GNUC__
    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "0" (1), "2" (0)
    );
#else
    __cpuidex(info, 1, 0);
#endif
    if ((info[2] & (1 << 27)) == 0 || (info[2] & (1 << 28)) == 0) return 0;
    unsigned long long xcr;
#ifdef __GNUC__
    xcr = __builtin_ia32_xgetbv(0);
#else
    xcr = _xgetbv(0);
#endif
    if ((xcr & 6) != 6) return 0;
    if ((xcr & 0xE0) != 0xE0) return 0;
#ifdef __GNUC__
    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "0" (7), "2" (0)
    );
#else
    __cpuidex(info, 7, 0);
#endif
    return (info[1] & (1 << 16)) != 0;
}

static inline int has_sse2() {
    int info[4];
#ifdef __GNUC__
    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "0" (1), "2" (0)
    );
#else
    __cpuidex(info, 1, 0);
#endif
    return (info[3] & (1 << 26)) != 0;
}

static inline int has_neon() {
#if defined(__arm__) || defined(__aarch64__)
#ifdef __linux__
    unsigned long hwcap = getauxval(AT_HWCAP);
#ifdef __aarch64__
    return (hwcap & HWCAP_ASIMD) != 0;
#else
    return (hwcap & HWCAP_NEON) != 0;
#endif
#elif defined(__APPLE__)
    int ret = 0;
    size_t len = sizeof(ret);
    sysctlbyname("hw.optional.neon", &ret, &len, NULL, 0);
    return ret;
#elif defined(_WIN32)
    return IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE);
#else
    return 0;
#endif
#else
    return 0;
#endif
}

static inline int has_vsx() {
#if defined(__powerpc__) || defined(__powerpc64__)
#ifdef __linux__
    return (getauxval(AT_HWCAP) & PPC_FEATURE_HAS_VSX);
#else
    return 1; // Assume yes on other platforms
#endif
#else
    return 0;
#endif
}

static inline int has_rvv() {
#if defined(__riscv)
#if defined(__riscv_vector)
    return 1;
#endif
#endif
    return 0;
}

typedef size_t (*url_escape_func_t)(const char * restrict src, size_t srclen, char * restrict dst, size_t dstcap);
typedef void * (*memmem_func_t)(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

static uint8_t url_safe[256];

static void init_url_safe(void) {
    memset(url_safe, 0, sizeof(url_safe));
    for (char c = 'a'; c <= 'z'; ++c) url_safe[(unsigned char)c] = 1;
    for (char c = 'A'; c <= 'Z'; ++c) url_safe[(unsigned char)c] = 1;
    for (char c = '0'; c <= '9'; ++c) url_safe[(unsigned char)c] = 1;
    url_safe['-'] = 1;
    url_safe['_'] = 1;
    url_safe['.'] = 1;
    url_safe['~'] = 1;
}

static inline size_t url_escape_scalar(const char * restrict src, size_t srclen, char * restrict dst, size_t dstcap) {
    const unsigned char *s = (const unsigned char *)src;
    char *d = dst;
    size_t o = 0;
    static const char hex[16] = "0123456789ABCDEF";

    size_t i = 0;
    for (; i + 15 < srclen; i += 16) {
        for (int j = 0; j < 16; ++j) {
            unsigned char c = s[i + j];
            if (likely(url_safe[c])) {
                if (unlikely(o + 1 >= dstcap)) return 0;
                d[o++] = (char)c;
            } else {
                if (unlikely(o + 3 >= dstcap)) return 0;
                d[o++] = '%';
                d[o++] = hex[c >> 4];
                d[o++] = hex[c & 15];
            }
        }
    }
    for (; i < srclen; ++i) {
        unsigned char c = s[i];
        if (likely(url_safe[c])) {
            if (unlikely(o + 1 >= dstcap)) return 0;
            d[o++] = (char)c;
        } else {
            if (unlikely(o + 3 >= dstcap)) return 0;
            d[o++] = '%';
            d[o++] = hex[c >> 4];
            d[o++] = hex[c & 15];
        }
    }
    d[o] = 0;
    return o;
}

#if defined(__x86_64__) || defined(_M_X64)

__attribute__((target("sse2")))
static size_t url_escape_sse2(const char * restrict src, size_t srclen, char * restrict dst, size_t dstcap) {
    const char *s = src;
    char *d = dst;
    size_t o = 0;
    static const char hex[16] = "0123456789ABCDEF";

    const char *end = src + srclen;

    while (s < end) {
        size_t remain = end - s;
        if (remain < 16 || o + remain * 3 >= dstcap) {
            while (s < end) {
                unsigned char c = *(unsigned char *)s++;
                if (likely(url_safe[c])) {
                    if (unlikely(o + 1 >= dstcap)) return 0;
                    *d++ = (char)c;
                    o++;
                } else {
                    if (unlikely(o + 3 >= dstcap)) return 0;
                    *d++ = '%';
                    *d++ = hex[c >> 4];
                    *d++ = hex[c & 15];
                    o += 3;
                }
            }
            break;
        }

        __m128i vec = _mm_loadu_si128((const __m128i *)s);
        __m128i lower_a = _mm_set1_epi8('a' - 1);
        __m128i upper_z = _mm_set1_epi8('z' + 1);
        __m128i lower_A = _mm_set1_epi8('A' - 1);
        __m128i upper_Z = _mm_set1_epi8('Z' + 1);
        __m128i lower_0 = _mm_set1_epi8('0' - 1);
        __m128i upper_9 = _mm_set1_epi8('9' + 1);

        __m128i az = _mm_and_si128(_mm_cmpgt_epi8(vec, lower_a), _mm_cmpgt_epi8(upper_z, vec));
        __m128i AZ = _mm_and_si128(_mm_cmpgt_epi8(vec, lower_A), _mm_cmpgt_epi8(upper_Z, vec));
        __m128i num = _mm_and_si128(_mm_cmpgt_epi8(vec, lower_0), _mm_cmpgt_epi8(upper_9, vec));
        __m128i dash = _mm_cmpeq_epi8(vec, _mm_set1_epi8('-'));
        __m128i under = _mm_cmpeq_epi8(vec, _mm_set1_epi8('_'));
        __m128i dot = _mm_cmpeq_epi8(vec, _mm_set1_epi8('.'));
        __m128i tilde = _mm_cmpeq_epi8(vec, _mm_set1_epi8('~'));

        __m128i good = _mm_or_si128(_mm_or_si128(_mm_or_si128(az, AZ), _mm_or_si128(num, dash)),
                                    _mm_or_si128(_mm_or_si128(under, dot), tilde));

        int mask = _mm_movemask_epi8(good);
        if (mask == 0xFFFF) {
            _mm_storeu_si128((__m128i *)d, vec);
            s += 16;
            d += 16;
            o += 16;
        } else {
            int bad_mask = ~mask;
            int pos = __builtin_ctz(bad_mask);
            memcpy(d, s, pos);
            d += pos;
            s += pos;
            o += pos;
            unsigned char c = *(unsigned char *)s++;
            if (unlikely(o + 3 >= dstcap)) return 0;
            *d++ = '%';
            *d++ = hex[c >> 4];
            *d++ = hex[c & 15];
            o += 3;
        }
    }
    *d = 0;
    return o;
}

#ifdef __AVX2__
__attribute__((target("avx2")))
static size_t url_escape_avx2(const char * restrict src, size_t srclen, char * restrict dst, size_t dstcap) {
    const char *s = src;
    char *d = dst;
    size_t o = 0;
    static const char hex[16] = "0123456789ABCDEF";

    const char *end = src + srclen;

    while (s < end) {
        size_t remain = end - s;
        if (remain < 32 || o + remain * 3 >= dstcap) {
            while (s < end) {
                unsigned char c = *(unsigned char *)s++;
                if (likely(url_safe[c])) {
                    if (unlikely(o + 1 >= dstcap)) return 0;
                    *d++ = (char)c;
                    o++;
                } else {
                    if (unlikely(o + 3 >= dstcap)) return 0;
                    *d++ = '%';
                    *d++ = hex[c >> 4];
                    *d++ = hex[c & 15];
                    o += 3;
                }
            }
            break;
        }

        __m256i vec = _mm256_loadu_si256((const __m256i *)s);
        __m256i lower_a = _mm256_set1_epi8('a' - 1);
        __m256i upper_z = _mm256_set1_epi8('z' + 1);
        __m256i lower_A = _mm256_set1_epi8('A' - 1);
        __m256i upper_Z = _mm256_set1_epi8('Z' + 1);
        __m256i lower_0 = _mm256_set1_epi8('0' - 1);
        __m256i upper_9 = _mm256_set1_epi8('9' + 1);

        __m256i az = _mm256_and_si256(_mm256_cmpgt_epi8(vec, lower_a), _mm256_cmpgt_epi8(upper_z, vec));
        __m256i AZ = _mm256_and_si256(_mm256_cmpgt_epi8(vec, lower_A), _mm256_cmpgt_epi8(upper_Z, vec));
        __m256i num = _mm256_and_si256(_mm256_cmpgt_epi8(vec, lower_0), _mm256_cmpgt_epi8(upper_9, vec));
        __m256i dash = _mm256_cmpeq_epi8(vec, _mm256_set1_epi8('-'));
        __m256i under = _mm256_cmpeq_epi8(vec, _mm256_set1_epi8('_'));
        __m256i dot = _mm256_cmpeq_epi8(vec, _mm256_set1_epi8('.'));
        __m256i tilde = _mm256_cmpeq_epi8(vec, _mm256_set1_epi8('~'));

        __m256i good = _mm256_or_si256(_mm256_or_si256(_mm256_or_si256(az, AZ), _mm256_or_si256(num, dash)),
                                       _mm256_or_si256(_mm256_or_si256(under, dot), tilde));

        int mask = _mm256_movemask_epi8(good);
        if (mask == -1) {
            _mm256_storeu_si256((__m256i *)d, vec);
            s += 32;
            d += 32;
            o += 32;
        } else {
            int bad_mask = ~mask;
            int pos = __builtin_ctz(bad_mask);
            for (int i = 0; i < pos; ++i) {
                *d++ = *s++;
            }
            o += pos;
            unsigned char c = *(unsigned char *)s++;
            if (unlikely(o + 3 >= dstcap)) return 0;
            *d++ = '%';
            *d++ = hex[c >> 4];
            *d++ = hex[c & 15];
            o += 3;
        }
    }
    *d = 0;
    return o;
}
#endif

#ifdef __AVX512F__
__attribute__((target("avx512f")))
static size_t url_escape_avx512(const char * restrict src, size_t srclen, char * restrict dst, size_t dstcap) {
    const char *s = src;
    char *d = dst;
    size_t o = 0;
    static const char hex[16] = "0123456789ABCDEF";

    const char *end = src + srclen;

    while (s < end) {
        size_t remain = end - s;
        if (remain < 64 || o + remain * 3 >= dstcap) {
            while (s < end) {
                unsigned char c = *(unsigned char *)s++;
                if (likely(url_safe[c])) {
                    if (unlikely(o + 1 >= dstcap)) return 0;
                    *d++ = (char)c;
                    o++;
                } else {
                    if (unlikely(o + 3 >= dstcap)) return 0;
                    *d++ = '%';
                    *d++ = hex[c >> 4];
                    *d++ = hex[c & 15];
                    o += 3;
                }
            }
            break;
        }

        __m512i vec = _mm512_loadu_si512((const __m512i *)s);
        __m512i lower_a = _mm512_set1_epi8('a' - 1);
        __m512i upper_z = _mm512_set1_epi8('z' + 1);
        __m512i lower_A = _mm512_set1_epi8('A' - 1);
        __m512i upper_Z = _mm512_set1_epi8('Z' + 1);
        __m512i lower_0 = _mm512_set1_epi8('0' - 1);
        __m512i upper_9 = _mm512_set1_epi8('9' + 1);

        __mmask64 az_mask = _mm512_cmpgt_epi8_mask(vec, lower_a) & _mm512_cmpgt_epi8_mask(upper_z, vec);
        __mmask64 AZ_mask = _mm512_cmpgt_epi8_mask(vec, lower_A) & _mm512_cmpgt_epi8_mask(upper_Z, vec);
        __mmask64 num_mask = _mm512_cmpgt_epi8_mask(vec, lower_0) & _mm512_cmpgt_epi8_mask(upper_9, vec);
        __mmask64 dash_mask = _mm512_cmpeq_epi8_mask(vec, _mm512_set1_epi8('-'));
        __mmask64 under_mask = _mm512_cmpeq_epi8_mask(vec, _mm512_set1_epi8('_'));
        __mmask64 dot_mask = _mm512_cmpeq_epi8_mask(vec, _mm512_set1_epi8('.'));
        __mmask64 tilde_mask = _mm512_cmpeq_epi8_mask(vec, _mm512_set1_epi8('~'));

        __mmask64 good_mask = az_mask | AZ_mask | num_mask | dash_mask | under_mask | dot_mask | tilde_mask;

        if (good_mask == 0xFFFFFFFFFFFFFFFFULL) {
            _mm512_storeu_si512((__m512i *)d, vec);
            s += 64;
            d += 64;
            o += 64;
        } else {
            uint64_t bad_mask = ~good_mask;
            int pos = __builtin_ctzll(bad_mask);
            for (int i = 0; i < pos; ++i) {
                *d++ = *s++;
            }
            o += pos;
            unsigned char c = *(unsigned char *)s++;
            if (unlikely(o + 3 >= dstcap)) return 0;
            *d++ = '%';
            *d++ = hex[c >> 4];
            *d++ = hex[c & 15];
            o += 3;
        }
    }
    *d = 0;
    return o;
}
#endif

__attribute__((target("sse2")))
static void *memmem_sse2(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (needlelen == 0) return (void*)haystack;
    if (haystacklen < needlelen) return NULL;
    const char *h = haystack;
    const char *n = needle;
    const char *end = h + haystacklen - needlelen + 1;
    char first_byte = n[0];
    size_t i = 0;
    for (; i + 16 <= haystacklen - needlelen + 1; i += 16) {
        __m128i block = _mm_loadu_si128((const __m128i*)(h + i));
        __m128i first = _mm_set1_epi8(first_byte);
        __m128i eq = _mm_cmpeq_epi8(block, first);
        int mask = _mm_movemask_epi8(eq);
        while (mask) {
            int bit = __builtin_ctz(mask);
            if (memcmp(h + i + bit, n, needlelen) == 0) {
                return (void*)(h + i + bit);
            }
            mask &= ~(1 << bit);
        }
    }
    for (; i < haystacklen - needlelen + 1; ++i) {
        if (h[i] == first_byte && memcmp(h + i, n, needlelen) == 0) {
            return (void*)(h + i);
        }
    }
    return NULL;
}

#ifdef __AVX2__
__attribute__((target("avx2")))
static void *memmem_avx2(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (needlelen == 0) return (void*)haystack;
    if (haystacklen < needlelen) return NULL;
    const char *h = haystack;
    const char *n = needle;
    const char *end = h + haystacklen - needlelen + 1;
    char first_byte = n[0];
    size_t i = 0;
    for (; i + 32 <= haystacklen - needlelen + 1; i += 32) {
        __m256i block = _mm256_loadu_si256((const __m256i*)(h + i));
        __m256i first = _mm256_set1_epi8(first_byte);
        __m256i eq = _mm256_cmpeq_epi8(block, first);
        int mask = _mm256_movemask_epi8(eq);
        while (mask) {
            int bit = __builtin_ctz(mask);
            if (memcmp(h + i + bit, n, needlelen) == 0) {
                return (void*)(h + i + bit);
            }
            mask &= ~(1 << bit);
        }
    }
    for (; i < haystacklen - needlelen + 1; ++i) {
        if (h[i] == first_byte && memcmp(h + i, n, needlelen) == 0) {
            return (void*)(h + i);
        }
    }
    return NULL;
}
#endif

#ifdef __AVX512F__
__attribute__((target("avx512f")))
static void *memmem_avx512(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (needlelen == 0) return (void*)haystack;
    if (haystacklen < needlelen) return NULL;
    const char *h = haystack;
    const char *n = needle;
    const char *end = h + haystacklen - needlelen + 1;
    char first_byte = n[0];
    size_t i = 0;
    for (; i + 64 <= haystacklen - needlelen + 1; i += 64) {
        __m512i block = _mm512_loadu_si512((const __m512i*)(h + i));
        __m512i first = _mm512_set1_epi8(first_byte);
        __mmask64 mask = _mm512_cmpeq_epi8_mask(block, first);
        while (mask) {
            int bit = __builtin_ctzll(mask);
            if (memcmp(h + i + bit, n, needlelen) == 0) {
                return (void*)(h + i + bit);
            }
            mask &= ~(1ULL << bit);
        }
    }
    for (; i < haystacklen - needlelen + 1; ++i) {
        if (h[i] == first_byte && memcmp(h + i, n, needlelen) == 0) {
            return (void*)(h + i);
        }
    }
    return NULL;
}
#endif
#endif

#if defined(__ARM_NEON) || defined(__aarch64__)
static size_t url_escape_neon(const char * restrict src, size_t srclen, char * restrict dst, size_t dstcap) {
    const char *s = src;
    char *d = dst;
    size_t o = 0;
    static const char hex[16] = "0123456789ABCDEF";

    const char *end = src + srclen;

    while (s < end) {
        size_t remain = end - s;
        if (remain < 16 || o + remain * 3 >= dstcap) {
            while (s < end) {
                unsigned char c = *(unsigned char *)s++;
                if (likely(url_safe[c])) {
                    if (unlikely(o + 1 >= dstcap)) return 0;
                    *d++ = (char)c;
                    o++;
                } else {
                    if (unlikely(o + 3 >= dstcap)) return 0;
                    *d++ = '%';
                    *d++ = hex[c >> 4];
                    *d++ = hex[c & 15];
                    o += 3;
                }
            }
            break;
        }

        uint8x16_t vec = vld1q_u8((const uint8_t *)s);
        uint8x16_t az = vandq_u8(vcgtq_u8(vec, vdupq_n_u8('a' - 1)), vcltq_u8(vec, vdupq_n_u8('z' + 1)));
        uint8x16_t AZ = vandq_u8(vcgtq_u8(vec, vdupq_n_u8('A' - 1)), vcltq_u8(vec, vdupq_n_u8('Z' + 1)));
        uint8x16_t num = vandq_u8(vcgtq_u8(vec, vdupq_n_u8('0' - 1)), vcltq_u8(vec, vdupq_n_u8('9' + 1)));
        uint8x16_t dash = vceqq_u8(vec, vdupq_n_u8('-'));
        uint8x16_t under = vceqq_u8(vec, vdupq_n_u8('_'));
        uint8x16_t dot = vceqq_u8(vec, vdupq_n_u8('.'));
        uint8x16_t tilde = vceqq_u8(vec, vdupq_n_u8('~'));

        uint8x16_t good = vorrq_u8(vorrq_u8(vorrq_u8(az, AZ), vorrq_u8(num, dash)),
                                   vorrq_u8(vorrq_u8(under, dot), tilde));
        uint8x16_t bad = vmvnq_u8(good);

        uint8x16_t bits = vshrq_n_u8(bad, 7);
        uint8_t bytes[16];
        vst1q_u8(bytes, bits);
        int mask = 0;
        for (int i = 0; i < 16; ++i) {
            if (bytes[i]) mask |= (1 << i);
        }

        if (mask == 0) {
            vst1q_u8((uint8_t *)d, vec);
            s += 16;
            d += 16;
            o += 16;
        } else {
            int pos = __builtin_ctz(mask);
            for (int i = 0; i < pos; ++i) {
                *d++ = *s++;
            }
            o += pos;
            unsigned char c = *(unsigned char *)s++;
            if (unlikely(o + 3 >= dstcap)) return 0;
            *d++ = '%';
            *d++ = hex[c >> 4];
            *d++ = hex[c & 15];
            o += 3;
        }
    }
    *d = 0;
    return o;
}

static void *memmem_neon(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (needlelen == 0) return (void*)haystack;
    if (haystacklen < needlelen) return NULL;
    const char *h = haystack;
    const char *n = needle;
    const char *end = h + haystacklen - needlelen + 1;
    char first_byte = n[0];
    const char *s = h;
    while (s < end) {
        size_t remain = end - s;
        if (remain < 16) break;
        uint8x16_t vec = vld1q_u8((const uint8_t*)s);
        uint8x16_t first = vdupq_n_u8(first_byte);
        uint8x16_t eq = vceqq_u8(vec, first);
        uint8x16_t bits = vshrq_n_u8(eq, 7);
        uint8_t bytes[16];
        vst1q_u8(bytes, bits);
        int mask = 0;
        for (int i = 0; i < 16; ++i) {
            if (bytes[i]) mask |= (1 << i);
        }
        if (mask == 0) {
            s += 16;
            continue;
        }
        // Scalar check in this chunk
        const char *chunk_end = s + 16 - needlelen + 1;
        if (chunk_end > end) chunk_end = end;
        for (const char *p = s; p < chunk_end; ++p) {
            if (*p == first_byte && memcmp(p, n, needlelen) == 0) {
                return (void*)p;
            }
        }
        s += 16;
    }
    // Tail
    for (const char *p = s; p < end; ++p) {
        if (*p == first_byte && memcmp(p, n, needlelen) == 0) {
            return (void*)p;
        }
    }
    return NULL;
}
#endif

#if defined(__powerpc__) || defined(__powerpc64__)
__attribute__((target("vsx")))
static size_t url_escape_vsx(const char * restrict src, size_t srclen, char * restrict dst, size_t dstcap) {
    const char *s = src;
    char *d = dst;
    size_t o = 0;
    static const char hex[16] = "0123456789ABCDEF";

    const char *end = src + srclen;

    while (s < end) {
        size_t remain = end - s;
        if (remain < 16 || o + remain * 3 >= dstcap) {
            while (s < end) {
                unsigned char c = *(unsigned char *)s++;
                if (likely(url_safe[c])) {
                    if (unlikely(o + 1 >= dstcap)) return 0;
                    *d++ = (char)c;
                    o++;
                } else {
                    if (unlikely(o + 3 >= dstcap)) return 0;
                    *d++ = '%';
                    *d++ = hex[c >> 4];
                    *d++ = hex[c & 15];
                    o += 3;
                }
            }
            break;
        }

        vector unsigned char vec = vec_ld(0, (unsigned char *)s);
        vector signed char lower_a = vec_splats((signed char)('a' - 1));
        vector signed char upper_z = vec_splats((signed char)('z' + 1));
        vector signed char lower_A = vec_splats((signed char)('A' - 1));
        vector signed char upper_Z = vec_splats((signed char)('Z' + 1));
        vector signed char lower_0 = vec_splats((signed char)('0' - 1));
        vector signed char upper_9 = vec_splats((signed char)('9' + 1));
        vector signed char v_vec = (vector signed char)vec;

        vector bool char az = vec_and(vec_cmpgt(v_vec, lower_a), vec_cmpgt(upper_z, v_vec));
        vector bool char AZ = vec_and(vec_cmpgt(v_vec, lower_A), vec_cmpgt(upper_Z, v_vec));
        vector bool char num = vec_and(vec_cmpgt(v_vec, lower_0), vec_cmpgt(upper_9, v_vec));
        vector bool char dash = vec_cmpeq(v_vec, vec_splats((signed char)'-'));
        vector bool char under = vec_cmpeq(v_vec, vec_splats((signed char)'_'));
        vector bool char dot = vec_cmpeq(v_vec, vec_splats((signed char)'.'));
        vector bool char tilde = vec_cmpeq(v_vec, vec_splats((signed char)'~'));

        vector bool char good = vec_or(vec_or(vec_or(az, AZ), vec_or(num, dash)),
                                      vec_or(vec_or(under, dot), tilde));
        vector bool char bad = vec_not(good);

        unsigned char bytes[16];
        vec_st((vector unsigned char)bad, 0, bytes);
        int mask = 0;
        for (int i = 0; i < 16; ++i) {
            if (bytes[i] == 0xFF) mask |= (1 << i);
        }

        if (mask == 0) {
            vec_st(vec, 0, (unsigned char *)d);
            s += 16;
            d += 16;
            o += 16;
        } else {
            int pos = __builtin_ctz(mask);
            memcpy(d, s, pos);
            d += pos;
            s += pos;
            o += pos;
            unsigned char c = *(unsigned char *)s++;
            if (unlikely(o + 3 >= dstcap)) return 0;
            *d++ = '%';
            *d++ = hex[c >> 4];
            *d++ = hex[c & 15];
            o += 3;
        }
    }
    *d = 0;
    return o;
}

__attribute__((target("vsx")))
static void *memmem_vsx(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (needlelen == 0) return (void*)haystack;
    if (haystacklen < needlelen) return NULL;
    const char *h = haystack;
    const char *n = needle;
    const char *end = h + haystacklen - needlelen + 1;
    char first_byte = n[0];
    const char *s = h;
    while (s < end) {
        size_t remain = end - s;
        if (remain < 16) break;
        vector unsigned char vec = vec_ld(0, (unsigned char*)s);
        vector signed char first = vec_splats((signed char)first_byte);
        vector signed char v_vec = (vector signed char)vec;
        vector bool char eq = vec_cmpeq(v_vec, first);
        unsigned char bytes[16];
        vec_st((vector unsigned char)eq, 0, bytes);
        int mask = 0;
        for (int i = 0; i < 16; ++i) {
            if (bytes[i] == 0xFF) mask |= (1 << i);
        }
        if (mask == 0) {
            s += 16;
            continue;
        }
        // Scalar check in this chunk
        const char *chunk_end = s + 16 - needlelen + 1;
        if (chunk_end > end) chunk_end = end;
        for (const char *p = s; p < chunk_end; ++p) {
            if (*p == first_byte && memcmp(p, n, needlelen) == 0) {
                return (void*)p;
            }
        }
        s += 16;
    }
    // Tail
    for (const char *p = s; p < end; ++p) {
        if (*p == first_byte && memcmp(p, n, needlelen) == 0) {
            return (void*)p;
        }
    }
    return NULL;
}
#endif

#if defined(__riscv) && defined(__riscv_vector)
__attribute__((target("v")))
static size_t url_escape_rvv(const char * restrict src, size_t srclen, char * restrict dst, size_t dstcap) {
    const char *s = src;
    char *d = dst;
    size_t o = 0;
    static const char hex[16] = "0123456789ABCDEF";

    size_t remain = srclen;
    while (remain > 0) {
        size_t vl = vsetvl_e8m1(remain);
        if (o + vl * 3 >= dstcap) return 0;

        vuint8m1_t vec = vle8_v_u8m1((const uint8_t *)s, vl);

        vbool8_t az = vmor_mm_b8(vmsgtu_vx_u8m1_b8(vec, 'a' - 1, vl), vmsltu_vx_u8m1_b8(vec, 'z' + 1, vl), vl);
        vbool8_t AZ = vmor_mm_b8(vmsgtu_vx_u8m1_b8(vec, 'A' - 1, vl), vmsltu_vx_u8m1_b8(vec, 'Z' + 1, vl), vl);
        vbool8_t num = vmor_mm_b8(vmsgtu_vx_u8m1_b8(vec, '0' - 1, vl), vmsltu_vx_u8m1_b8(vec, '9' + 1, vl), vl);
        vbool8_t dash = vmseq_vx_u8m1_b8(vec, '-', vl);
        vbool8_t under = vmseq_vx_u8m1_b8(vec, '_', vl);
        vbool8_t dot = vmseq_vx_u8m1_b8(vec, '.', vl);
        vbool8_t tilde = vmseq_vx_u8m1_b8(vec, '~', vl);

        vbool8_t good = vmor_mm_b8(vmor_mm_b8(vmor_mm_b8(az, AZ, vl), vmor_mm_b8(num, dash, vl), vl),
                                   vmor_mm_b8(vmor_mm_b8(under, dot, vl), tilde, vl), vl);
        vbool8_t bad = vmnot_m_b8(good, vl);

        ssize_t pos = vfirst_m_b8(bad, vl);
        if (pos == -1) {
            vse8_v_u8m1((uint8_t *)d, vec, vl);
            s += vl;
            d += vl;
            o += vl;
            remain -= vl;
        } else {
            if (pos > 0) {
                vuint8m1_t sub_vec = vslidedown_vx_u8m1(vec, 0, pos, vl);
                vse8_v_u8m1((uint8_t *)d, sub_vec, pos);
                d += pos;
                o += pos;
            }
            s += pos;
            remain -= pos;
            unsigned char c = *(unsigned char *)s++;
            remain--;
            if (unlikely(o + 3 >= dstcap)) return 0;
            *d++ = '%';
            *d++ = hex[c >> 4];
            *d++ = hex[c & 15];
            o += 3;
        }
    }
    *d = 0;
    return o;
}

__attribute__((target("v")))
static void *memmem_rvv(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) {
    if (needlelen == 0) return (void*)haystack;
    if (haystacklen < needlelen) return NULL;
    const char *h = haystack;
    const char *n = needle;
    const char *end = h + haystacklen - needlelen + 1;
    char first_byte = n[0];
    const char *s = h;
    size_t remain = haystacklen - needlelen + 1;
    while (remain > 0) {
        size_t vl = vsetvl_e8m1(remain);
        vuint8m1_t vec = vle8_v_u8m1((const uint8_t*)s, vl);
        vbool8_t eq = vmseq_vx_u8m1_b8(vec, first_byte, vl);
        ssize_t pos = vfirst_m_b8(eq, vl);
        if (pos == -1) {
            s += vl;
            remain -= vl;
            continue;
        }
        // Has potential, scalar check
        const char *chunk_end = s + vl - needlelen + 1;
        if (chunk_end > end) chunk_end = end;
        for (const char *p = s; p < chunk_end; ++p) {
            if (*p == first_byte && memcmp(p, n, needlelen) == 0) {
                return (void*)p;
            }
        }
        s += vl;
        remain -= vl;
    }
    return NULL;
}
#endif

int main(int argc, char **argv) {
#ifdef _WIN32
    QueryPerformanceFrequency(&g_freq);
#endif
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
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (unlikely(WSAStartup(MAKEWORD(2,2), &wsa) != 0)) {
        WRITE_FD(STDERR_FD, "WSAStartup failed\n", 18);
        return 1;
    }
#endif

    if (verbose) t_start = current_time_ns();
    if (verbose) t_init_end = current_time_ns();

    init_url_safe();

    url_escape_func_t url_escape_func = url_escape_scalar;
    memmem_func_t memmem_func = memmem_scalar;
#if defined(__riscv) && defined(__riscv_vector)
    if (has_rvv()) {
        url_escape_func = url_escape_rvv;
        memmem_func = memmem_rvv;
    }
#endif
#if defined(__powerpc__) || defined(__powerpc64__)
    if (has_vsx()) {
        url_escape_func = url_escape_vsx;
        memmem_func = memmem_vsx;
    }
#endif
#if defined(__x86_64__) || defined(_M_X64)
    #ifdef __AVX512F__
    if (has_avx512()) {
        url_escape_func = url_escape_avx512;
        memmem_func = memmem_avx512;
    } else
    #endif
    #ifdef __AVX2__
    if (has_avx2()) {
        url_escape_func = url_escape_avx2;
        memmem_func = memmem_avx2;
    } else
    #endif
    if (has_sse2()) {
        url_escape_func = url_escape_sse2;
        memmem_func = memmem_sse2;
    }
#endif
#if defined(__ARM_NEON) || defined(__aarch64__)
    if (has_neon()) {
        url_escape_func = url_escape_neon;
        memmem_func = memmem_neon;
    }
#endif

#if defined(_MSC_VER)
    __declspec(align(64))
#elif defined(__GNUC__) || defined(__clang__)
    char escaped[MAX_ESCAPED_SIZE] __attribute__((aligned(64)));
    char request[MAX_REQ_SIZE] __attribute__((aligned(64)));
    char response_buffer[RESPONSE_BUFFER_SIZE] __attribute__((aligned(64)));
#else
    char escaped[MAX_ESCAPED_SIZE];
    char request[MAX_REQ_SIZE];
    char response_buffer[RESPONSE_BUFFER_SIZE];
#endif

    if (verbose) t_escape_start = current_time_ns();
    size_t query_len = strlen(search_term);
    size_t escaped_len = url_escape_func(search_term, query_len, escaped, sizeof(escaped));
    if (unlikely(escaped_len == 0)) {
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
    static const char suffix[] = " HTTP/1.1\r\nHost: apibay.org\r\nUser-Agent: fastclient/1.0\r\nConnection: close\r\nAccept: */*\r\n\r\n";
    size_t suf_len = sizeof(suffix) - 1;
    if (pre_len + escaped_len + suf_len >= sizeof(request)) {
        WRITE_FD(STDERR_FD, "Request too long\n", 17);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    memcpy(request, prefix, pre_len);
    memcpy(request + pre_len, escaped, escaped_len);
    memcpy(request + pre_len + escaped_len, suffix, suf_len);
    size_t req_len = pre_len + escaped_len + suf_len;

    if (verbose) t_url_end = current_time_ns();

    const char *ips[] = {"172.67.137.143", "104.21.62.171"};
    const int num_ips = 2;

    if (verbose) t_setup_start = current_time_ns();

    sock_t s = INVALID_SOCKET;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);

    if (verbose) t_setup_end = current_time_ns();

    if (verbose) t_perform_start = current_time_ns();

    bool success = false;

    for (int i = 0; i < num_ips; ++i) {
        addr.sin_addr.s_addr = inet_addr(ips[i]);

        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) continue;

        int one = 1;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));

#ifdef TCP_FASTOPEN_CONNECT
        int tfo = 1;
        setsockopt(s, IPPROTO_TCP, TCP_FASTOPEN_CONNECT, (const char*)&tfo, sizeof(tfo));
#elif defined(TCP_FASTOPEN)
        int tfo = 1;
        setsockopt(s, IPPROTO_TCP, TCP_FASTOPEN, (const char*)&tfo, sizeof(tfo));
#endif

#ifdef _WIN32
        setsockopt(s, IPPROTO_TCP, TCP_FASTOPEN, (const char*)&one, sizeof(one));
#endif

        int bufsize = 1048576;
        setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char*)&bufsize, sizeof(bufsize));
        setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char*)&bufsize, sizeof(bufsize));

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

#ifdef __linux__
        int quickack = 1;
        setsockopt(s, IPPROTO_TCP, TCP_QUICKACK, (const char*)&quickack, sizeof(quickack));
#endif

#ifdef _WIN32
        // Load ConnectEx for TFO with early data
        LPFN_CONNECTEX ConnectEx = NULL;
        GUID guid = WSAID_CONNECTEX;
        DWORD ioctl_bytes;
        if (WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx, sizeof(ConnectEx), &ioctl_bytes, NULL, NULL) != 0) {
            close_sock(s);
            continue;
        }
        // Bind to any address
        struct sockaddr_in bind_addr;
        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        bind_addr.sin_port = 0;
        if (bind(s, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) == SOCKET_ERROR) {
            close_sock(s);
            continue;
        }
        // Use overlapped ConnectEx with data
        OVERLAPPED ol = {0};
        ol.hEvent = WSACreateEvent();
        if (ol.hEvent == WSA_INVALID_EVENT) {
            close_sock(s);
            continue;
        }
        DWORD bytes_sent = 0;
        BOOL res = ConnectEx(s, (struct sockaddr*)&addr, sizeof(addr), request, (DWORD)req_len, &bytes_sent, &ol);
        if (!res) {
            int err = WSAGetLastError();
            if (err != WSA_IO_PENDING) {
                WSACloseEvent(ol.hEvent);
                close_sock(s);
                continue;
            }
        }
        DWORD wait_res = WaitForSingleObject(ol.hEvent, 5000);
        if (wait_res != WAIT_OBJECT_0) {
            WSACloseEvent(ol.hEvent);
            close_sock(s);
            continue;
        }
        DWORD transfer_bytes;
        if (!WSAGetOverlappedResult(s, &ol, &transfer_bytes, TRUE, &transfer_bytes)) {
            WSACloseEvent(ol.hEvent);
            close_sock(s);
            continue;
        }
        WSACloseEvent(ol.hEvent);
        if (bytes_sent != req_len) {
            // Fallback to send remaining
            WSABUF bufs[1];
            bufs[0].buf = request + bytes_sent;
            bufs[0].len = (ULONG)(req_len - bytes_sent);
            DWORD remaining_sent = 0;
            int ret = WSASend(s, bufs, 1, &remaining_sent, 0, NULL, NULL);
            if (ret == SOCKET_ERROR || remaining_sent != req_len - bytes_sent) {
                close_sock(s);
                continue;
            }
        }
        success = true;
        break;
#else
        struct iovec iov[1];
        iov[0].iov_base = request;
        iov[0].iov_len = req_len;

        struct msghdr msg = {0};
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        size_t total_to_send = req_len;
        size_t total_sent = 0;

        int flags = 0;
#ifdef MSG_FASTOPEN
        flags = MSG_FASTOPEN;
        msg.msg_name = (void *)&addr;
        msg.msg_namelen = sizeof(addr);
#endif
        bool connected = false;

        while (total_sent < total_to_send) {
            ssize_t sent = sendmsg(s, &msg, flags);
            if (sent <= 0) {
                if (sent == 0) break;
                int err = errno;
                if (connected || (err != EINPROGRESS && err != EAGAIN)) {
                    if (!connected && err == EOPNOTSUPP) {
                        // fallback to connect
                        msg.msg_name = NULL;
                        msg.msg_namelen = 0;
                        flags = 0;
                        if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
                            close_sock(s);
                            s = INVALID_SOCKET;
                            break;
                        }
                        connected = true;
                        continue;
                    } else {
                        close_sock(s);
                        s = INVALID_SOCKET;
                        break;
                    }
                }
                // wait for writable
                fd_set wfd;
                FD_ZERO(&wfd);
                FD_SET(s, &wfd);
                struct timeval tv_select = {5, 0};
                int sel = select((int)s + 1, NULL, &wfd, NULL, &tv_select);
                if (sel <= 0) {
                    close_sock(s);
                    s = INVALID_SOCKET;
                    break;
                }
                int so_error;
                socklen_t len = sizeof(so_error);
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so_error, &len);
                if (so_error != 0) {
                    close_sock(s);
                    s = INVALID_SOCKET;
                    break;
                }
                connected = true;
                msg.msg_name = NULL;
                msg.msg_namelen = 0;
                flags = 0;
                continue;
            }
            total_sent += (size_t)sent;
            while (sent > 0 && msg.msg_iovlen > 0) {
                if (sent < (ssize_t)msg.msg_iov[0].iov_len) {
                    msg.msg_iov[0].iov_base = (char *)msg.msg_iov[0].iov_base + sent;
                    msg.msg_iov[0].iov_len -= (size_t)sent;
                    sent = 0;
                } else {
                    sent -= (ssize_t)msg.msg_iov[0].iov_len;
                    msg.msg_iov++;
                    msg.msg_iovlen--;
                }
            }
        }

        if (total_sent == total_to_send) {
            success = true;
            break;
        } else {
            close_sock(s);
            s = INVALID_SOCKET;
            continue;
        }
#endif
    }

    if (!success || s == INVALID_SOCKET) {
        WRITE_FD(STDERR_FD, "Connect failed\n", 15);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    size_t received = 0;
    ssize_t content_length = -1;
    bool headers_parsed = false;

    for (;;) {
        ssize_t r = recv(s, response_buffer + received, (int)(sizeof(response_buffer) - 1 - received), 0);
        if (r > 0) {
            received += (size_t)r;
            if (!headers_parsed && received > 4) {
                char *headers_end = (char *)memmem_func(response_buffer, received, "\r\n\r\n", 4);
                if (headers_end) {
                    size_t headers_len = headers_end - response_buffer + 4;
                    char *cl_start = (char *)memmem_func(response_buffer, headers_len, "Content-Length:", 15);
                    if (cl_start) {
                        cl_start += 15;
                        while (*cl_start == ' ') cl_start++;
                        content_length = atoi(cl_start);
                    }
                    headers_parsed = true;
                    if (content_length >= 0 && received >= headers_len + (size_t)content_length) break;
                }
            }
            if (headers_parsed && content_length >= 0 && received >= (size_t)content_length + (received - (size_t)content_length)) break;
            if (received >= sizeof(response_buffer) - 1) break;
        } else break;
#ifdef __linux__
        int quickack = 1;
        setsockopt(s, IPPROTO_TCP, TCP_QUICKACK, (const char*)&quickack, sizeof(quickack));
#endif
    }

    if (verbose) t_perform_end = current_time_ns();

#ifdef _WIN32
    shutdown(s, SD_BOTH);
#else
    shutdown(s, SHUT_RDWR);
#endif
    close_sock(s);

    if (received > 0 && !verbose) {
        response_buffer[received] = 0;
        WRITE_FD(STDOUT_FD, response_buffer, (unsigned int)received);
        WRITE_FD(STDOUT_FD, "\n", 1);
    }

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
