/* =============================================================================
 * OpenSweet OS - Minimal Libc & Stubs for Doom Port (user/doom_libc.c)
 * ============================================================================= */

#include "opensweet.h"
#include <stdarg.h>

/* Forward declarations */
int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/* =============================================================================
 * ctype & errno
 * ============================================================================= */
static int s_errno = 0;
int *__errno(void) { return &s_errno; }
void *__imp__errno = (void*)__errno;

int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}
int tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

int abs(int x) {
    return x < 0 ? -x : x;
}

long labs(long x) {
    return x < 0 ? -x : x;
}

/* __imp_ pointers expected by MinGW dllimport references */
void *__imp_toupper = (void*)toupper;
void *__imp_tolower = (void*)tolower;

/* =============================================================================
 * Strings
 * ============================================================================= */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int _stricmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower(*(const unsigned char*)s1);
        int c2 = tolower(*(const unsigned char*)s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower(*(const unsigned char*)s1) - tolower(*(const unsigned char*)s2);
}
int strcasecmp(const char *s1, const char *s2) { return _stricmp(s1, s2); }

int _strnicmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && *s2) {
        int c1 = tolower(*(const unsigned char*)s1);
        int c2 = tolower(*(const unsigned char*)s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return tolower(*(const unsigned char*)s1) - tolower(*(const unsigned char*)s2);
}
int strncasecmp(const char *s1, const char *s2, size_t n) { return _strnicmp(s1, s2, n); }

void *__imp__stricmp = (void*)_stricmp;
void *__imp__strnicmp = (void*)_strnicmp;

char *strncpy(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (n && (*dst++ = *src++)) n--;
    while (n--) *dst++ = '\0';
    return ret;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char*)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n) {
                h++;
                n++;
            }
            if (!*n) return (char*)haystack;
        }
    }
    return NULL;
}

char *strdup(const char *s) {
    size_t len = os_strlen(s);
    char *copy = (char*)os_malloc(len + 1);
    if (copy) {
        os_memcpy(copy, s, len + 1);
    }
    return copy;
}

int atoi(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return sign * val;
}

double atof(const char *s) {
    return (double)atoi(s);
}

char *getenv(const char *name) {
    (void)name;
    return NULL;
}

int mkdir(const char *path) {
    (void)path;
    return 0;
}

void exit(int code) {
    os_exit(code);
    while (1) {}
}

/* =============================================================================
 * FILE & I/O Emulation
 * ============================================================================= */
typedef struct {
    uint8_t *data;
    size_t size;
    size_t pos;
    int is_write;
    int is_cached;
    char path[64];
} fake_file_t;

#define MAX_FAKE_FILES 16
static fake_file_t s_files[MAX_FAKE_FILES];
static fake_file_t s_std_streams[3]; /* stdin, stdout, stderr */
uint8_t *s_wad_cache = NULL;
size_t s_wad_cache_len = 0;

void *__acrt_iob_func(unsigned index) {
    if (index < 3) return (void*)&s_std_streams[index];
    return NULL;
}
void *__imp___acrt_iob_func = (void*)__acrt_iob_func;

void *fopen(const char *filename, const char *mode) {
    int write_mode = (strrchr(mode, 'w') != NULL || strrchr(mode, 'a') != NULL);
    
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < MAX_FAKE_FILES; i++) {
        if (!s_files[i].data && s_files[i].size == 0) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return NULL;

    fake_file_t *f = &s_files[slot];
    os_memset(f, 0, sizeof(*f));
    strncpy(f->path, filename, sizeof(f->path) - 1);
    f->is_write = write_mode;

    if (!write_mode) {
        const char *clean_name = filename;
        while (*clean_name == '/' || (*clean_name == '.' && *(clean_name + 1) == '/')) {
            if (*clean_name == '/') clean_name++;
            else if (*clean_name == '.' && *(clean_name + 1) == '/') clean_name += 2;
        }

        /* Check if opening WAD file */
        if (strstr(clean_name, ".wad") != NULL || strstr(clean_name, ".WAD") != NULL) {
            if (!s_wad_cache) {
                printf("[doom] Loading WAD file into memory...\n");
                size_t wad_alloc = 4196020 + 4096; /* 4.2 MB */
                void *buf = os_malloc(wad_alloc);
                if (!buf) {
                    printf("[doom] Error: failed to allocate memory for WAD!\n");
                    return NULL;
                }
                ssize_t bytes = os_read_file("/doom1.wad", buf, wad_alloc);
                if (bytes <= 0) {
                    bytes = os_read_file(filename, buf, wad_alloc);
                }
                if (bytes <= 0) {
                    printf("[doom] Error: failed to read WAD from disk! ret=%i\n", (int)bytes);
                    os_free(buf);
                    return NULL;
                }
                s_wad_cache = (uint8_t*)buf;
                s_wad_cache_len = (size_t)bytes;
                printf("[doom] WAD cached successfully: %i bytes at %p\n", (int)s_wad_cache_len, s_wad_cache);
            }
            f->data = s_wad_cache;
            f->size = s_wad_cache_len;
            f->pos = 0;
            f->is_cached = 1;
            return (void*)f;
        }

        /* For non-WAD files (configs, etc.), try loading with 64KB buffer */
        size_t cap = 65536;
        void *buf = os_malloc(cap);
        if (!buf) return NULL;

        ssize_t bytes = os_read_file(filename, buf, cap);
        if (bytes < 0) {
            char alt_path[64];
            alt_path[0] = '/';
            strncpy(alt_path + 1, clean_name, sizeof(alt_path) - 2);
            bytes = os_read_file(alt_path, buf, cap);
        }
        if (bytes < 0) {
            os_free(buf);
            return NULL; /* File does not exist */
        }

        f->data = (uint8_t*)buf;
        f->size = (size_t)bytes;
        f->pos = 0;
        f->is_cached = 0;
    } else {
        /* Write buffer starts at 64KB, dynamically expands */
        f->size = 65536;
        f->data = (uint8_t*)os_malloc(f->size);
        f->pos = 0;
        f->is_cached = 0;
    }

    return (void*)f;
}

int fclose(void *stream) {
    if (!stream) return -1;
    fake_file_t *f = (fake_file_t*)stream;
    if (f->is_write && f->data && f->pos > 0) {
        os_write_file(f->path, f->data, f->pos);
    }
    if (f->data && !f->is_cached) {
        os_free(f->data);
    }
    f->data = NULL;
    f->size = 0;
    f->pos = 0;
    f->is_cached = 0;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t count, void *stream) {
    if (!stream || !ptr || size == 0 || count == 0) return 0;
    fake_file_t *f = (fake_file_t*)stream;
    size_t to_read = size * count;
    if (f->pos + to_read > f->size) {
        to_read = (f->pos < f->size) ? (f->size - f->pos) : 0;
    }
    if (to_read > 0) {
        os_memcpy(ptr, f->data + f->pos, to_read);
        f->pos += to_read;
    }
    return to_read / size;
}

size_t fwrite(const void *ptr, size_t size, size_t count, void *stream) {
    if (!stream || !ptr) return 0;
    fake_file_t *f = (fake_file_t*)stream;
    if (f == (fake_file_t*)__acrt_iob_func(1) || f == (fake_file_t*)__acrt_iob_func(2)) {
        /* stdout or stderr */
        size_t total = size * count;
        os_print((const char*)ptr);
        return count;
    }
    size_t bytes = size * count;
    if (f->pos + bytes > f->size) {
        size_t new_cap = f->size * 2 + bytes;
        void *new_data = os_realloc(f->data, new_cap);
        if (!new_data) return 0;
        f->data = (uint8_t*)new_data;
        f->size = new_cap;
    }
    os_memcpy(f->data + f->pos, ptr, bytes);
    f->pos += bytes;
    return count;
}

int fseek(void *stream, long offset, int whence) {
    if (!stream) return -1;
    fake_file_t *f = (fake_file_t*)stream;
    long new_pos = 0;
    if (whence == 0) { /* SEEK_SET */
        new_pos = offset;
    } else if (whence == 1) { /* SEEK_CUR */
        new_pos = (long)f->pos + offset;
    } else if (whence == 2) { /* SEEK_END */
        new_pos = (long)f->size + offset;
    }
    if (new_pos < 0) new_pos = 0;
    if ((size_t)new_pos > f->size) new_pos = (long)f->size;
    f->pos = (size_t)new_pos;
    return 0;
}

long ftell(void *stream) {
    if (!stream) return -1;
    return (long)((fake_file_t*)stream)->pos;
}

int fflush(void *stream) {
    (void)stream;
    return 0;
}

int remove(const char *filename) {
    (void)filename;
    return 0;
}

int rename(const char *oldname, const char *newname) {
    (void)oldname;
    (void)newname;
    return 0;
}

int puts(const char *str) {
    os_print(str);
    os_print("\n");
    return 0;
}

int putchar(int c) {
    char buf[2] = { (char)c, 0 };
    os_print(buf);
    return c;
}

int fputs(const char *str, void *stream) {
    (void)stream;
    os_print(str);
    return 0;
}

/* =============================================================================
 * Formatted output (printf / snprintf / sscanf)
 * ============================================================================= */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    if (!buf || size == 0) return 0;
    size_t out_idx = 0;

    while (*fmt && out_idx + 1 < size) {
        if (*fmt != '%') {
            buf[out_idx++] = *fmt++;
            continue;
        }
        fmt++; /* skip '%' */

        /* parse flags / width */
        int width = 0;
        int pad_zero = 0;
        int precision = -1;
        while (*fmt == '0') { pad_zero = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                precision = precision * 10 + (*fmt - '0');
                fmt++;
            }
        }
        if (*fmt == 'l' || *fmt == 'h') fmt++;

        char spec = *fmt++;
        if (spec == 's') {
            const char *str = va_arg(args, const char*);
            if (!str) str = "(null)";
            while (*str && out_idx + 1 < size) {
                buf[out_idx++] = *str++;
            }
        } else if (spec == 'd' || spec == 'i') {
            int val = va_arg(args, int);
            char num_buf[32];
            os_itoa(val, num_buf);
            int len = (int)os_strlen(num_buf);
            int digits = (num_buf[0] == '-') ? (len - 1) : len;
            int pad_zeros = (precision > digits) ? (precision - digits) : 0;
            if (precision < 0 && pad_zero && width > len) {
                pad_zeros = width - len;
            }
            int total_len = len + pad_zeros;
            while (width > total_len && out_idx + 1 < size) {
                buf[out_idx++] = ' ';
                width--;
            }
            int start_d = 0;
            if (num_buf[0] == '-') {
                if (out_idx + 1 < size) buf[out_idx++] = '-';
                start_d = 1;
            }
            while (pad_zeros > 0 && out_idx + 1 < size) {
                buf[out_idx++] = '0';
                pad_zeros--;
            }
            for (int j = start_d; j < len && out_idx + 1 < size; j++) {
                buf[out_idx++] = num_buf[j];
            }
        } else if (spec == 'u') {
            unsigned int val = va_arg(args, unsigned int);
            char num_buf[32];
            int p = 0;
            if (val == 0) {
                num_buf[p++] = '0';
            } else {
                char temp[20];
                int tp = 0;
                while (val > 0) {
                    temp[tp++] = '0' + (val % 10);
                    val /= 10;
                }
                while (tp > 0) num_buf[p++] = temp[--tp];
            }
            num_buf[p] = 0;
            int len = p;
            int pad_zeros = (precision > len) ? (precision - len) : 0;
            if (precision < 0 && pad_zero && width > len) pad_zeros = width - len;
            int total_len = len + pad_zeros;
            while (width > total_len && out_idx + 1 < size) {
                buf[out_idx++] = ' ';
                width--;
            }
            while (pad_zeros > 0 && out_idx + 1 < size) {
                buf[out_idx++] = '0';
                pad_zeros--;
            }
            for (int j = 0; j < len && out_idx + 1 < size; j++) {
                buf[out_idx++] = num_buf[j];
            }
        } else if (spec == 'x' || spec == 'X' || spec == 'p') {
            uint64_t val = (spec == 'p') ? (uint64_t)va_arg(args, void*) : (uint64_t)va_arg(args, unsigned int);
            char num_buf[32];
            int p = 0;
            if (val == 0) {
                num_buf[p++] = '0';
            } else {
                char temp[20];
                int tp = 0;
                while (val > 0) {
                    int rem = val % 16;
                    temp[tp++] = rem < 10 ? ('0' + rem) : ('a' + rem - 10);
                    val /= 16;
                }
                while (tp > 0) num_buf[p++] = temp[--tp];
            }
            num_buf[p] = 0;
            for (int j = 0; j < p && out_idx + 1 < size; j++) {
                buf[out_idx++] = num_buf[j];
            }
        } else if (spec == 'c') {
            char c = (char)va_arg(args, int);
            buf[out_idx++] = c;
        } else if (spec == '%') {
            buf[out_idx++] = '%';
        }
    }
    buf[out_idx] = '\0';
    return (int)out_idx;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return res;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(buf, 4096, fmt, args);
    va_end(args);
    return res;
}

int printf(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    os_print(buf);
    return res;
}

int fprintf(void *stream, const char *fmt, ...) {
    (void)stream;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    os_print(buf);
    return res;
}

int vfprintf(void *stream, const char *fmt, va_list args) {
    (void)stream;
    char buf[1024];
    int res = vsnprintf(buf, sizeof(buf), fmt, args);
    os_print(buf);
    return res;
}

int sscanf(const char *src, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int assigned = 0;
    while (*fmt && *src) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd' || *fmt == 'i') {
                int *p = va_arg(args, int*);
                *p = atoi(src);
                assigned++;
                while (*src == ' ' || *src == '-' || (*src >= '0' && *src <= '9')) src++;
            } else if (*fmt == 's') {
                char *dst = va_arg(args, char*);
                while (*src && *src != ' ' && *src != '\n') *dst++ = *src++;
                *dst = 0;
                assigned++;
            }
            fmt++;
        } else {
            if (*fmt == *src) { fmt++; src++; }
            else break;
        }
    }
    va_end(args);
    return assigned;
}

/* Alias mingw printf symbol names */
int __mingw_snprintf(char *buf, size_t size, const char *fmt, ...) __attribute__((alias("snprintf")));
int __mingw_sprintf(char *buf, const char *fmt, ...) __attribute__((alias("sprintf")));
int __mingw_printf(const char *fmt, ...) __attribute__((alias("printf")));
int __mingw_fprintf(void *f, const char *fmt, ...) __attribute__((alias("fprintf")));
int __mingw_vfprintf(void *f, const char *fmt, va_list a) __attribute__((alias("vfprintf")));
int __mingw_sscanf(const char *s, const char *f, ...) __attribute__((alias("sscanf")));

void *__imp__vsnprintf = (void*)vsnprintf;

/* Windows stubs */
int __imp_MultiByteToWideChar = 0;
int __imp_WideCharToMultiByte = 0;
int __imp_MessageBoxW = 0;

