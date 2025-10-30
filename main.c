#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <ctype.h>

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define WRITE_FD _write
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#else
#include <unistd.h>
#include <sys/uio.h>
#define WRITE_FD write
#define STDOUT_FILENO STDOUT_FILENO
#define STDERR_FILENO STDERR_FILENO
#endif

#define RESPONSE_BUFFER_SIZE 65536UL
#define MAX_QUERY_LEN 1024
#define MAX_ESCAPED_SIZE ((MAX_QUERY_LEN * 3) + 1)
#define MAX_URL_LENGTH 2048

#ifndef likely
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif

struct MemoryStruct {
    char * restrict memory;
    size_t size;
    size_t capacity;
};

static inline size_t WriteMemoryCallback(void * restrict contents, size_t size, size_t nmemb, void * restrict userp) __attribute__((hot, always_inline));

static inline size_t WriteMemoryCallback(void * restrict contents, size_t size, size_t nmemb, void * restrict userp) {
    const size_t realsize = size * nmemb;
    struct MemoryStruct * restrict mem = (struct MemoryStruct * restrict)userp;

    const size_t new_size = mem->size + realsize;

    if (unlikely(new_size > mem->capacity)) {
        return 0;
    }

    __builtin_prefetch(mem->memory + mem->size, 1, 0);
    __builtin_memcpy(mem->memory + mem->size, contents, realsize);
    mem->size = new_size;

    return realsize;
}

static char *url_escape(const char *s) {
    size_t len = strlen(s);
    if (unlikely(len > MAX_QUERY_LEN)) {
        return NULL;
    }
    char out[MAX_ESCAPED_SIZE];
    char *p = out;
    unsigned char needs_escape[256];
    memset(needs_escape, 1, sizeof(needs_escape));
    for (char c = 'a'; c <= 'z'; ++c) needs_escape[(unsigned char)c] = 0;
    for (char c = 'A'; c <= 'Z'; ++c) needs_escape[(unsigned char)c] = 0;
    for (char c = '0'; c <= '9'; ++c) needs_escape[(unsigned char)c] = 0;
    needs_escape['-'] = 0;
    needs_escape['_'] = 0;
    needs_escape['.'] = 0;
    needs_escape['~'] = 0;
    static const char hex[] = "0123456789ABCDEF";
    for (const char *q = s; *q; q++) {
        unsigned char c = (unsigned char)*q;
        if (!needs_escape[c]) {
            *p++ = c;
        } else {
            *p++ = '%';
            *p++ = hex[c >> 4];
            *p++ = hex[c & 15];
        }
    }
    *p = 0;
    char *escaped = malloc(strlen(out) + 1);
    if (unlikely(!escaped)) return NULL;
    strcpy(escaped, out);
    return escaped;
}

int main(int argc, char *argv[]) {
    if (unlikely(argc != 2)) {
        const char *usage1 = "Usage: ";
        WRITE_FD(STDERR_FILENO, usage1, strlen(usage1));
        WRITE_FD(STDERR_FILENO, argv[0], strlen(argv[0]));
        const char *usage2 = " <search_term>\n";
        WRITE_FD(STDERR_FILENO, usage2, strlen(usage2));
        return 1;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (unlikely(WSAStartup(MAKEWORD(2,2), &wsaData) != 0)) {
        const char *msg = "WSAStartup failed\n";
        WRITE_FD(STDERR_FILENO, msg, strlen(msg));
        return 1;
    }
#endif

#ifdef __APPLE__
    (void)curl_global_sslset(CURLSSLBACKEND_SECURE_TRANSPORT, NULL, NULL);
#else
    (void)curl_global_sslset(CURLSSLBACKEND_OPENSSL, NULL, NULL);
#endif

#ifdef _WIN32
    curl_global_init(CURL_GLOBAL_SSL | CURL_GLOBAL_WIN32);
#else
    curl_global_init(CURL_GLOBAL_SSL);
#endif

    CURL *handle = curl_easy_init();
    if (unlikely(!handle)) {
        curl_global_cleanup();
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    char *escaped = url_escape(argv[1]);
    if (unlikely(!escaped)) {
        curl_easy_cleanup(handle);
        curl_global_cleanup();
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    char url[MAX_URL_LENGTH];
    int url_len = snprintf(url, sizeof(url), "https://apibay.org/q.php?q=%s", escaped);
    free(escaped);
    if (unlikely(url_len < 0 || (size_t)url_len >= sizeof(url))) {
        const char *msg = "URL too long\n";
        WRITE_FD(STDERR_FILENO, msg, strlen(msg));
        curl_easy_cleanup(handle);
        curl_global_cleanup();
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

#if defined(_MSC_VER)
    __declspec(align(64)) char response_buffer[RESPONSE_BUFFER_SIZE];
#elif defined(__GNUC__) || defined(__clang__)
    char response_buffer[RESPONSE_BUFFER_SIZE] __attribute__((aligned(64)));
#else
    char response_buffer[RESPONSE_BUFFER_SIZE];
#endif
    memset(response_buffer, 0, sizeof(response_buffer));

    struct MemoryStruct chunk;
    chunk.memory = response_buffer;
    chunk.size = 0;
    chunk.capacity = sizeof(response_buffer);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(handle, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(handle, CURLOPT_TCP_FASTOPEN, 1L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 0L);
    curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3);
    curl_easy_setopt(handle, CURLOPT_BUFFERSIZE, 1048576L);
    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_SESSIONID_CACHE, 0L);
    curl_easy_setopt(handle, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(handle, CURLOPT_FRESH_CONNECT, 1L);
    curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(handle, CURLOPT_PIPEWAIT, 1L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(handle);

    if (unlikely(res != CURLE_OK)) {
        const char *msg1 = "Request failed for ";
        WRITE_FD(STDERR_FILENO, msg1, strlen(msg1));
        WRITE_FD(STDERR_FILENO, url, strlen(url));
        const char *msg2 = ": ";
        WRITE_FD(STDERR_FILENO, msg2, strlen(msg2));
        const char *error_msg = curl_easy_strerror(res);
        WRITE_FD(STDERR_FILENO, error_msg, strlen(error_msg));
        const char *nl = "\n";
        WRITE_FD(STDERR_FILENO, nl, 1);
    } else {
        if (likely(chunk.size > 0)) {
            WRITE_FD(STDOUT_FILENO, chunk.memory, chunk.size);
            const char nl = '\n';
            WRITE_FD(STDOUT_FILENO, &nl, 1);
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(handle);
    curl_global_cleanup();
#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}