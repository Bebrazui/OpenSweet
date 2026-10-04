#ifndef OPENSWEET_H
#define OPENSWEET_H

/* =============================================================================
 * OpenSweet OS - Native C User-Space SDK & Runtime (include/opensweet.h)
 * Standard freestanding C development kit for Ring 3 applications on OpenSweet.
 * Provides:
 *   - Freestanding data types and standard utility routines
 *   - Fast 64-bit SYSCALL ABI wrappers (System V AMD64)
 *   - Desktop Window Management (sys_gui_create_win, poll, update, close)
 *   - 32bpp ARGB 2D Drawing Library & UI Widgets (Buttons, Panels, Typography)
 *   - Automatic CRT0 entry point (_start -> main)
 * ============================================================================= */

#define NULL ((void*)0)

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef unsigned long long size_t;
typedef signed long long   ssize_t;
typedef unsigned long long uintptr_t;

#if !defined(__bool_true_false_are_defined) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
typedef _Bool bool;
#define true  1
#define false 0
#endif

#include "opensweet_font_aa.h"

/* System Call Numbers */
#define SYS_EXIT           0
#define SYS_WRITE          1
#define SYS_READ           2
#define SYS_SLEEP          3
#define SYS_YIELD          4
#define SYS_UPTIME         5
#define SYS_GUI_CREATE_WIN 10
#define SYS_GUI_UPDATE_WIN 11
#define SYS_GUI_POLL_EVENT 12
#define SYS_GUI_CLOSE_WIN  13
#define SYS_APP_REGISTER   14
#define SYS_APP_INFO       15
#define SYS_LISTDIR        16
#define SYS_SPAWN          17
#define SYS_BRK            18
#define SYS_WRITE_FILE     19
#define SYS_READ_FILE      20
#define SYS_UNLINK         21
#define SYS_GET_TIME       22
#define SYS_SYSINFO        23
#define SYS_OPEN           24
#define SYS_CLOSE          25
#define SYS_LSEEK          26
#define SYS_PIPE           27
#define SYS_DUP2           28
#define SYS_WAITPID        29
#define SYS_GETCWD         30
#define SYS_CHDIR          31
#define SYS_SPAWN_STDIO    32

#define WNOHANG            1
#define WIFEXITED(s)       (((s) & 0x7f) == 0)
#define WEXITSTATUS(s)     (((s) & 0xff00) >> 8)

/* File System Directory Entry Structure */
typedef struct {
    uint32_t inode;
    uint32_t type;     /* 1 = file, 2 = dir */
    uint64_t size;     /* size in bytes */
    char     name[32]; /* null-terminated filename */
} os_dirent_t;

/* Application Package Magic ("OS_APP\0\0") */
#define OS_APP_MAGIC 0x00005050415F534FULL

/* Application Package Metadata Structure (embedded directly into ELF binary) */
typedef struct {
    uint64_t magic;                    /* OS_APP_MAGIC */
    char     name[24];                 /* Application display name: "Notepad" */
    char     version[12];              /* Version string: "1.0.0" */
    char     author[32];               /* Signature / Author: "OpenSweet Team [Verified]" */
    char     description[48];          /* Short description: "Modern GUI Text Editor" */
    char     exec_path[32];            /* Path to ELF binary on ext4: "/notepad.elf" */
    uint32_t icon_width;               /* Icon width (44) */
    uint32_t icon_height;              /* Icon height (44) */
    uint32_t icon_pixels[44 * 44];     /* Embedded 44x44 32bpp ARGB pixel array */
} os_app_package_t;

typedef os_app_package_t os_app_info_t;

/* GUI Event Types */
#define OS_EVENT_NONE       0
#define OS_EVENT_MOUSE_MOVE 1
#define OS_EVENT_MOUSE_DOWN 2
#define OS_EVENT_MOUSE_UP   3
#define OS_EVENT_KEY_DOWN   4
#define OS_EVENT_WIN_CLOSE    5
#define OS_EVENT_KEY_UP       6
#define OS_EVENT_WIN_MINIMIZE 7
#define OS_EVENT_WIN_MAXIMIZE 8

/* Special Key Codes (param for OS_EVENT_KEY_DOWN / OS_EVENT_KEY_UP) */
#define OS_KEY_UP           0x80
#define OS_KEY_DOWN         0x81
#define OS_KEY_LEFT         0x82
#define OS_KEY_RIGHT        0x83
#define OS_KEY_CTRL         0x84

/* Color Definitions (32bpp ARGB) */
#define OS_ARGB(a, r, g, b) (((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define OS_RGB(r, g, b)     OS_ARGB(0xFF, r, g, b)

#define OS_COLOR_OBSIDIAN  0xFF0A0E17
#define OS_COLOR_SURFACE   0xFF141C2B
#define OS_COLOR_CARD      0xFF1E293B
#define OS_COLOR_SLATE_950 0xFF020617
#define OS_COLOR_SLATE_900 0xFF0F172A
#define OS_COLOR_SLATE_850 0xFF172033
#define OS_COLOR_SLATE_800 0xFF1E293B
#define OS_COLOR_SLATE_700 0xFF334155
#define OS_COLOR_SLATE_600 0xFF475569
#define OS_COLOR_SLATE_500 0xFF64748B
#define OS_COLOR_SLATE_400 0xFF94A3B8
#define OS_COLOR_SLATE_300 0xFFCBD5E1
#define OS_COLOR_SLATE_200 0xFFE2E8F0
#define OS_COLOR_SLATE_100 0xFFF1F5F9
#define OS_COLOR_WHITE     0xFFFFFFFF
#define OS_COLOR_BLACK     0xFF000000

#define OS_COLOR_INDIGO    0xFF4F46E5
#define OS_COLOR_INDIGO_LT 0xFF6366F1
#define OS_COLOR_INDIGO_DK 0xFF3730A3
#define OS_COLOR_VIOLET    0xFF8B5CF6
#define OS_COLOR_EMERALD   0xFF10B981
#define OS_COLOR_EMERALD_LT 0xFF34D399
#define OS_COLOR_EMERALD_DK 0xFF065F46
#define OS_COLOR_AMBER     0xFFF59E0B
#define OS_COLOR_ROSE      0xFFF43F5E
#define OS_COLOR_CYAN      0xFF06B6D4
#define OS_COLOR_CYAN_NEON 0xFF22D3EE
#define OS_COLOR_SKY       0xFF0EA5E9

/* Window and Event Structures */
typedef struct {
    uint32_t type;         /* OS_EVENT_* */
    int32_t  x;            /* client-relative X */
    int32_t  y;            /* client-relative Y */
    uint32_t param;        /* buttons (1=LMB) or key ASCII code */
} os_event_t;

typedef struct {
    int32_t   win_id;
    uint32_t *canvas;
    int32_t   width;
    int32_t   height;
    int32_t   client_w;
    int32_t   client_h;
} os_window_t;

/* =============================================================================
 * Inline Syscall Architecture (System V AMD64 ABI)
 * ============================================================================= */
static inline int64_t os_syscall0(int64_t num) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t os_syscall1(int64_t num, int64_t a1) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t os_syscall2(int64_t num, int64_t a1, int64_t a2) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t os_syscall3(int64_t num, int64_t a1, int64_t a2, int64_t a3) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t os_syscall5(int64_t num, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5) {
    int64_t ret;
    register int64_t r10 __asm__("r10") = a4;
    register int64_t r8  __asm__("r8")  = a5;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* =============================================================================
 * Standard System Call Functions
 * ============================================================================= */
static inline void os_exit(int code) {
    os_syscall1(SYS_EXIT, code);
    while (1) { __asm__ volatile ("pause"); }
}

static inline ssize_t os_write(int fd, const void *buf, size_t count) {
    return (ssize_t)os_syscall3(SYS_WRITE, fd, (int64_t)buf, count);
}

static inline ssize_t os_read(int fd, void *buf, size_t count) {
    return (ssize_t)os_syscall3(SYS_READ, fd, (int64_t)buf, count);
}

static inline void os_sleep(uint32_t ms) {
    os_syscall1(SYS_SLEEP, ms);
}

static inline void os_yield(void) {
    os_syscall0(SYS_YIELD);
}

static inline uint32_t os_uptime(void) {
    return (uint32_t)os_syscall0(SYS_UPTIME);
}

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} os_time_t;

/* Read true hardware Real-Time Clock (RTC CMOS) */
static inline os_time_t os_get_time(void) {
    uint64_t val = (uint64_t)os_syscall0(SYS_GET_TIME);
    os_time_t t;
    t.hours = (uint8_t)((val >> 16) & 0xFF);
    t.minutes = (uint8_t)((val >> 8) & 0xFF);
    t.seconds = (uint8_t)(val & 0xFF);
    return t;
}

/* System Hardware & Kernel Diagnostics Structure */
typedef struct {
    uint32_t total_ram_mb;   /* Total physical RAM in MB (256) */
    uint32_t free_ram_mb;    /* Free physical RAM in MB */
    uint32_t used_ram_mb;    /* Used physical RAM in MB */
    uint32_t total_pages;    /* Total PMM 4KB pages (65536) */
    uint32_t free_pages;     /* Free PMM 4KB pages */
    uint32_t screen_w;       /* Display resolution width (1920) */
    uint32_t screen_h;       /* Display resolution height (1080) */
    uint32_t screen_bpp;     /* Display color depth (32) */
    uint32_t task_count;     /* Active tasks in scheduler */
    char     kernel_ver[32]; /* Kernel version string */
    char     os_name[32];    /* OS name string */
    char     wm_name[32];    /* Window Manager / Compositor name */
} os_sysinfo_t;

static inline int os_get_sysinfo(os_sysinfo_t *info) {
    if (!info) return -1;
    return (int)os_syscall1(SYS_SYSINFO, (int64_t)info);
}


/* =============================================================================
 * Window Management API
 * ============================================================================= */
static inline os_window_t os_create_window(const char *title, int x, int y, int w, int h) {
    /* H2 Mitigation: Clamp dimensions to guaranteed 2MB canvas bounds [200..960] x [150..560] */
    if (w < 200) w = 200;
    if (w > 960) w = 960;
    if (h < 150) h = 150;
    if (h > 560) h = 560;

    os_window_t win;
    win.win_id = -1;
    win.canvas = NULL;
    win.width = w;
    win.height = h;
    win.client_w = w;
    win.client_h = h > 33 ? h - 33 : 0;

    int64_t res = os_syscall5(SYS_GUI_CREATE_WIN, (int64_t)title, x, y, w, h);
    if (res == -1) return win;

    win.win_id = (int32_t)(res & 0xFFFFFFFF);
    win.canvas = (uint32_t*)(uintptr_t)((uint64_t)res >> 32);
    return win;
}

static inline void os_update_window(os_window_t *win) {
    if (!win || win->win_id < 0) return;
    os_syscall1(SYS_GUI_UPDATE_WIN, win->win_id);
}

static inline int os_poll_event(os_window_t *win, os_event_t *ev) {
    if (!win || win->win_id < 0 || !ev) return 0;
    return (int)os_syscall2(SYS_GUI_POLL_EVENT, win->win_id, (int64_t)ev);
}

static inline void os_close_window(os_window_t *win) {
    if (!win || win->win_id < 0) return;
    os_syscall1(SYS_GUI_CLOSE_WIN, win->win_id);
    win->win_id = -1;
    win->canvas = NULL;
}

/* =============================================================================
 * Application Lifecycle & Package Registration API
 * ============================================================================= */
static inline int os_register_app(const os_app_info_t *info) {
    if (!info) return -1;
    return (int)os_syscall2(SYS_APP_REGISTER, (int64_t)info, sizeof(os_app_info_t));
}

/* Directory listing from ext4 filesystem */
static inline int os_read_dir(os_dirent_t *entries, int max_entries) {
    if (!entries || max_entries <= 0) return -1;
    return (int)os_syscall2(SYS_LISTDIR, (int64_t)entries, max_entries);
}

/* Spawn an ELF application with command line arguments from ext4 storage */
static inline int os_spawn_args(const char *path, const char *const argv[]) {
    if (!path) return -1;
    return (int)os_syscall2(SYS_SPAWN, (int64_t)path, (int64_t)argv);
}

/* Spawn an ELF application from ext4 storage */
static inline int os_spawn(const char *path) {
    return os_spawn_args(path, (const char *const[]){ path, NULL });
}

/* Write/create a file on ext4 filesystem */
static inline ssize_t os_write_file(const char *path, const void *buf, size_t count) {
    if (!path) return -1;
    return (ssize_t)os_syscall3(SYS_WRITE_FILE, (int64_t)path, (int64_t)buf, (int64_t)count);
}

/* Read file content directly from ext4 filesystem into memory */
static inline ssize_t os_read_file(const char *path, void *buf, size_t max_count) {
    if (!path || !buf || max_count == 0) return -1;
    return (ssize_t)os_syscall3(SYS_READ_FILE, (int64_t)path, (int64_t)buf, (int64_t)max_count);
}

/* Delete/unlink a file from ext4 filesystem */
static inline int os_delete_file(const char *path) {
    if (!path) return -1;
    return (int)os_syscall1(SYS_UNLINK, (int64_t)path);
}

/* =============================================================================
 * POSIX File Descriptor API (open, close, read, write, lseek)
 * ============================================================================= */
static inline int os_open(const char *path, int flags, int mode) {
    if (!path) return -1;
    return (int)os_syscall3(SYS_OPEN, (int64_t)path, flags, mode);
}

static inline int os_close(int fd) {
    return (int)os_syscall1(SYS_CLOSE, fd);
}

static inline ssize_t os_read_fd(int fd, void *buf, size_t count) {
    if (!buf || count == 0) return 0;
    return (ssize_t)os_syscall3(SYS_READ, fd, (int64_t)buf, (int64_t)count);
}

static inline ssize_t os_write_fd(int fd, const void *buf, size_t count) {
    if (!buf || count == 0) return 0;
    return (ssize_t)os_syscall3(SYS_WRITE, fd, (int64_t)buf, (int64_t)count);
}

static inline int64_t os_lseek(int fd, int64_t offset, int whence) {
    return (int64_t)os_syscall3(SYS_LSEEK, fd, offset, whence);
}

static inline int os_pipe(int pipefd[2]) {
    if (!pipefd) return -1;
    return (int)os_syscall1(SYS_PIPE, (int64_t)pipefd);
}

static inline int os_dup2(int oldfd, int newfd) {
    return (int)os_syscall2(SYS_DUP2, oldfd, newfd);
}

static inline int os_waitpid(int pid, int *status, int options) {
    return (int)os_syscall3(SYS_WAITPID, pid, (int64_t)status, options);
}

static inline int os_wait(int *status) {
    return os_waitpid(-1, status, 0);
}

/* Get current working directory path (POSIX getcwd) */
static inline char *os_getcwd(char *buf, size_t size) {
    if (!buf || size == 0) return NULL;
    int64_t res = os_syscall2(SYS_GETCWD, (int64_t)buf, (int64_t)size);
    if (res <= 0) return NULL;
    return (char*)(uintptr_t)res;
}

/* Change current working directory (POSIX chdir) */
static inline int os_chdir(const char *path) {
    if (!path) return -1;
    return (int)os_syscall1(SYS_CHDIR, (int64_t)path);
}

/* Spawn an ELF application with arguments and custom stdin/stdout/stderr redirections */
static inline int os_spawn_stdio(const char *path, const char *const argv[], int in_fd, int out_fd, int err_fd) {
    if (!path) return -1;
    return (int)os_syscall5(SYS_SPAWN_STDIO, (int64_t)path, (int64_t)argv, in_fd, out_fd, err_fd);
}
/* Adjust or query process heap break (sys_brk) */
static inline void *os_brk(void *addr) {
    return (void*)(uintptr_t)os_syscall1(SYS_BRK, (int64_t)addr);
}

/* =============================================================================
 * Dynamic Memory Allocation (malloc, free, realloc, calloc)
 * Simple, robust boundary-tag heap allocator built on top of os_brk.
 * ============================================================================= */
typedef struct os_mem_block {
    size_t size;                /* Size of user data area */
    int is_free;                /* 1 if free, 0 if allocated */
    struct os_mem_block *next;  /* Pointer to next block */
} os_mem_block_t;

#define OS_MEM_BLOCK_HEADER_SIZE sizeof(os_mem_block_t)

static os_mem_block_t *os_heap_head = NULL;

static inline void *os_malloc(size_t size) {
    if (size == 0) return NULL;

    /* Align size to 16 bytes */
    size = (size + 15) & ~((size_t)15);

    /* Search for first matching free block */
    os_mem_block_t *curr = os_heap_head;
    os_mem_block_t *prev = NULL;

    while (curr) {
        if (curr->is_free && curr->size >= size) {
            /* Split block if extra capacity is large enough */
            if (curr->size >= size + OS_MEM_BLOCK_HEADER_SIZE + 16) {
                os_mem_block_t *split = (os_mem_block_t*)((uint8_t*)(curr + 1) + size);
                split->size = curr->size - size - OS_MEM_BLOCK_HEADER_SIZE;
                split->is_free = 1;
                split->next = curr->next;
                curr->next = split;
                curr->size = size;
            }
            curr->is_free = 0;
            return (void*)(curr + 1);
        }
        prev = curr;
        curr = curr->next;
    }

    /* No free block large enough; expand heap via os_brk */
    void *current_brk = os_brk(NULL);
    if (!current_brk || (int64_t)(uintptr_t)current_brk == -1) return NULL;

    size_t total_needed = OS_MEM_BLOCK_HEADER_SIZE + size;
    void *new_brk = (void*)((uint8_t*)current_brk + total_needed);
    void *allocated_brk = os_brk(new_brk);
    if (allocated_brk != new_brk) {
        return NULL; /* Out of memory or brk limit exceeded */
    }

    os_mem_block_t *block = (os_mem_block_t*)current_brk;
    block->size = size;
    block->is_free = 0;
    block->next = NULL;

    if (prev) {
        prev->next = block;
    } else {
        os_heap_head = block;
    }

    return (void*)(block + 1);
}

static inline void os_free(void *ptr) {
    if (!ptr) return;

    os_mem_block_t *block = ((os_mem_block_t*)ptr) - 1;
    block->is_free = 1;

    /* Coalesce physically adjacent free blocks */
    os_mem_block_t *curr = os_heap_head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            uint8_t *expected_next = (uint8_t*)(curr + 1) + curr->size;
            if ((uint8_t*)curr->next == expected_next) {
                curr->size += OS_MEM_BLOCK_HEADER_SIZE + curr->next->size;
                curr->next = curr->next->next;
                continue; /* Check if next block can also be coalesced */
            }
        }
        curr = curr->next;
    }
}

static inline void *os_realloc(void *ptr, size_t new_size) {
    if (!ptr) return os_malloc(new_size);
    if (new_size == 0) {
        os_free(ptr);
        return NULL;
    }

    os_mem_block_t *block = ((os_mem_block_t*)ptr) - 1;
    if (block->size >= new_size) {
        return ptr; /* Already fits */
    }

    void *new_ptr = os_malloc(new_size);
    if (!new_ptr) return NULL;

    /* Copy existing data (safely bounded by smaller of old and new size) */
    size_t copy_len = (block->size < new_size) ? block->size : new_size;
    uint8_t *dst = (uint8_t*)new_ptr;
    const uint8_t *src = (const uint8_t*)ptr;
    for (size_t i = 0; i < copy_len; i++) {
        dst[i] = src[i];
    }

    os_free(ptr);
    return new_ptr;
}

static inline void *os_calloc(size_t num, size_t size) {
    /* L2 Mitigation: Prevent multiplication overflow on allocation size */
    if (size != 0 && num > ((size_t)-1) / size) return NULL;
    size_t total = num * size;
    void *ptr = os_malloc(total);
    if (ptr) {
        uint8_t *p = (uint8_t*)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

/* Standard C library aliases */
__attribute__((weak)) void *malloc(size_t size) {
    return os_malloc(size);
}

__attribute__((weak)) void free(void *ptr) {
    os_free(ptr);
}

__attribute__((weak)) void *realloc(void *ptr, size_t size) {
    return os_realloc(ptr, size);
}

__attribute__((weak)) void *calloc(size_t num, size_t size) {
    return os_calloc(num, size);
}

/* =============================================================================
 * Minimal Freestanding C Runtime Utilities
 * ============================================================================= */
static inline void *os_memset(void *dst, int c, size_t n) {
    uint8_t *p = (uint8_t*)dst;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return dst;
}

static inline void *os_memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

static inline size_t os_strlen(const char *s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static inline char *os_strcpy(char *dst, const char *src) {
    if (!dst || !src) return dst;
    char *orig = dst;
    while ((*dst++ = *src++));
    return orig;
}

static inline int os_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* Standard C compiler intrinsic aliases */
__attribute__((weak)) void *memset(void *dst, int c, size_t n) {
    return os_memset(dst, c, n);
}

__attribute__((weak)) void *memcpy(void *dst, const void *src, size_t n) {
    return os_memcpy(dst, src, n);
}

__attribute__((weak)) void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dst;
}

__attribute__((weak)) size_t strlen(const char *s) {
    return os_strlen(s);
}

static inline void os_itoa(int64_t n, char *buf) {
    if (!buf) return;
    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[32];
    int i = 0;
    int is_neg = 0;
    if (n < 0) {
        is_neg = 1;
        n = -n;
    }
    while (n > 0) {
        tmp[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    if (is_neg) tmp[i++] = '-';
    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

static inline void os_print(const char *s) {
    if (!s) return;
    os_write(1, s, os_strlen(s));
}

/* =============================================================================
 * POSIX Runtime Compatibility Layer (Foundations for Unix Shells & Utilities)
 * ============================================================================= */

static inline int os_strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static inline char *os_strncpy(char *dst, const char *src, size_t n) {
    if (!dst || !src) return dst;
    char *orig = dst;
    while (n && (*dst++ = *src++)) n--;
    while (n--) *dst++ = '\0';
    return orig;
}

/* 1. Environment Variables & CWD */
static char os_env_table[16][128] = {
    "PATH=/bin:/",
    "HOME=/",
    "USER=root",
    "TERM=xterm-256color",
    "SHELL=/terminal.elf",
    "PWD=/",
    "OS=OpenSweet"
};
static int os_env_count = 7;
static char *os_environ_ptrs[17] = {
    os_env_table[0], os_env_table[1], os_env_table[2], os_env_table[3],
    os_env_table[4], os_env_table[5], os_env_table[6], NULL
};
static char **environ = os_environ_ptrs;

static char os_cwd[128] = "/";

static inline char *getenv(const char *name) {
    if (!name) return NULL;
    size_t len = os_strlen(name);
    for (int i = 0; i < os_env_count; i++) {
        if (os_strncmp(os_env_table[i], name, len) == 0 && os_env_table[i][len] == '=') {
            return os_env_table[i] + len + 1;
        }
    }
    return NULL;
}

static inline int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !value) return -1;
    size_t nlen = os_strlen(name);
    size_t vlen = os_strlen(value);
    /* L2 Mitigation: Enforce 128-byte slot limit (name + '=' + value + '\0') */
    if (nlen + 1 + vlen >= 128) return -1;
    for (int i = 0; i < os_env_count; i++) {
        if (os_strncmp(os_env_table[i], name, nlen) == 0 && os_env_table[i][nlen] == '=') {
            if (!overwrite) return 0;
            os_strcpy(os_env_table[i] + nlen + 1, value);
            return 0;
        }
    }
    if (os_env_count < 15) {
        char *p = os_env_table[os_env_count];
        os_strcpy(p, name);
        os_strcpy(p + nlen, "=");
        os_strcpy(p + nlen + 1, value);
        os_environ_ptrs[os_env_count] = p;
        os_env_count++;
        os_environ_ptrs[os_env_count] = NULL;
        return 0;
    }
    return -1;
}

static inline char *getcwd(char *buf, size_t size) {
    char *res = os_getcwd(buf, size);
    if (res) {
        os_strncpy(os_cwd, res, sizeof(os_cwd) - 1);
        setenv("PWD", os_cwd, 1);
    }
    return res;
}

static inline int chdir(const char *path) {
    if (!path) return -1;
    int res = os_chdir(path);
    if (res == 0) {
        os_getcwd(os_cwd, sizeof(os_cwd));
        setenv("PWD", os_cwd, 1);
    }
    return res;
}

/* 2. Standard POSIX File Descriptors (0=stdin, 1=stdout, 2=stderr, 3..15=files) */
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200
#define O_APPEND 0x0400

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct {
    int      is_open;
    int      flags;
    char     path[64];
    size_t   offset;
    size_t   size;
    uint8_t *data;
} os_fd_entry_t;

static os_fd_entry_t os_fd_table[16];
static int os_fd_initialized = 0;

static inline void os_fd_init(void) {
    if (os_fd_initialized) return;
    os_fd_table[0].is_open = 1;
    os_fd_table[1].is_open = 1;
    os_fd_table[2].is_open = 1;
    os_fd_initialized = 1;
}

static inline int isatty(int fd) {
    return (fd >= 0 && fd <= 2);
}

static inline int open(const char *pathname, int flags, ...) {
    os_fd_init();
    if (!pathname) return -1;
    int slot = -1;
    for (int i = 3; i < 16; i++) {
        if (!os_fd_table[i].is_open) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -24;

    os_fd_table[slot].is_open = 1;
    os_fd_table[slot].flags = flags;
    os_strncpy(os_fd_table[slot].path, pathname, 63);
    os_fd_table[slot].offset = 0;
    os_fd_table[slot].size = 0;
    os_fd_table[slot].data = (uint8_t*)os_malloc(65536);
    if (os_fd_table[slot].data) {
        ssize_t n = os_read_file(pathname, os_fd_table[slot].data, 65536);
        if (n >= 0) os_fd_table[slot].size = (size_t)n;
    }
    return slot;
}

static inline int close(int fd) {
    os_fd_init();
    if (fd < 0 || fd >= 16 || !os_fd_table[fd].is_open) return -1;
    if (fd >= 3) {
        if ((os_fd_table[fd].flags & (O_WRONLY | O_RDWR)) && os_fd_table[fd].data) {
            os_write_file(os_fd_table[fd].path, os_fd_table[fd].data, os_fd_table[fd].size);
        }
        if (os_fd_table[fd].data) {
            os_free(os_fd_table[fd].data);
            os_fd_table[fd].data = NULL;
        }
        os_fd_table[fd].is_open = 0;
    }
    return 0;
}

static inline ssize_t read(int fd, void *buf, size_t count) {
    os_fd_init();
    if (fd < 0 || fd >= 16 || !os_fd_table[fd].is_open || !buf) return -1;
    if (fd == 0) return os_read(0, buf, count);
    if (fd >= 3 && os_fd_table[fd].data) {
        size_t rem = (os_fd_table[fd].offset < os_fd_table[fd].size) ?
                     (os_fd_table[fd].size - os_fd_table[fd].offset) : 0;
        if (count > rem) count = rem;
        if (count > 0) {
            os_memcpy(buf, os_fd_table[fd].data + os_fd_table[fd].offset, count);
            os_fd_table[fd].offset += count;
        }
        return count;
    }
    return 0;
}

static inline ssize_t write(int fd, const void *buf, size_t count) {
    os_fd_init();
    if (fd < 0 || fd >= 16 || !os_fd_table[fd].is_open || !buf) return -1;
    if (fd == 1 || fd == 2) return os_write(fd, buf, count);
    if (fd >= 3 && os_fd_table[fd].data) {
        if (os_fd_table[fd].offset + count > 65536) {
            count = 65536 - os_fd_table[fd].offset;
        }
        if (count > 0) {
            os_memcpy(os_fd_table[fd].data + os_fd_table[fd].offset, buf, count);
            os_fd_table[fd].offset += count;
            if (os_fd_table[fd].offset > os_fd_table[fd].size) {
                os_fd_table[fd].size = os_fd_table[fd].offset;
            }
        }
        return count;
    }
    return 0;
}

/* =============================================================================
 * 8x8 Monochrome Bitmap Font (ASCII 32 ' ' to 126 '~')
 * ============================================================================= */
static const uint8_t os_font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* 32 ' ' */
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, /* 33 '!' */
    {0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, /* 34 '"' */
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, /* 35 '#' */
    {0x18,0x7E,0xD8,0x7C,0x1B,0x7E,0x18,0x00}, /* 36 '$' */
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00}, /* 37 '%' */
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, /* 38 '&' */
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, /* 39 ''' */
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* 40 '(' */
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* 41 ')' */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* 42 '*' */
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* 43 '+' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, /* 44 ',' */
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* 45 '-' */
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* 46 '.' */
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, /* 47 '/' */
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, /* 48 '0' */
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 49 '1' */
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00}, /* 50 '2' */
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 51 '3' */
    {0x0C,0x1C,0x3C,0x6C,0xFE,0x0C,0x0C,0x00}, /* 52 '4' */
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 53 '5' */
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 54 '6' */
    {0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, /* 55 '7' */
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 56 '8' */
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, /* 57 '9' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, /* 58 ':' */
    {0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, /* 59 ';' */
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* 60 '<' */
    {0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00}, /* 61 '=' */
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, /* 62 '>' */
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, /* 63 '?' */
    {0x3C,0x66,0x6E,0x6A,0x6E,0x60,0x3C,0x00}, /* 64 '@' */
    {0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0x00}, /* 65 'A' */
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, /* 66 'B' */
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, /* 67 'C' */
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, /* 68 'D' */
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, /* 69 'E' */
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, /* 70 'F' */
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, /* 71 'G' */
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, /* 72 'H' */
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 73 'I' */
    {0x0E,0x06,0x06,0x06,0x06,0x66,0x3C,0x00}, /* 74 'J' */
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, /* 75 'K' */
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, /* 76 'L' */
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, /* 77 'M' */
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, /* 78 'N' */
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* 79 'O' */
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, /* 80 'P' */
    {0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00}, /* 81 'Q' */
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, /* 82 'R' */
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, /* 83 'S' */
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, /* 84 'T' */
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, /* 85 'U' */
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, /* 86 'V' */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* 87 'W' */
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, /* 88 'X' */
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, /* 89 'Y' */
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, /* 90 'Z' */
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* 91 '[' */
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, /* 92 '\' */
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, /* 93 ']' */
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, /* 94 '^' */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* 95 '_' */
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, /* 96 '`' */
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, /* 97 'a' */
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, /* 98 'b' */
    {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, /* 99 'c' */
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, /* 100 'd' */
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, /* 101 'e' */
    {0x1C,0x30,0x78,0x30,0x30,0x30,0x30,0x00}, /* 102 'f' */
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, /* 103 'g' */
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, /* 104 'h' */
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, /* 105 'i' */
    {0x06,0x00,0x06,0x06,0x06,0x06,0x66,0x3C}, /* 106 'j' */
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, /* 107 'k' */
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* 108 'l' */
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, /* 109 'm' */
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, /* 110 'n' */
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, /* 111 'o' */
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, /* 112 'p' */
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, /* 113 'q' */
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, /* 114 'r' */
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, /* 115 's' */
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0C,0x00}, /* 116 't' */
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, /* 117 'u' */
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, /* 118 'v' */
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, /* 119 'w' */
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, /* 120 'x' */
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, /* 121 'y' */
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, /* 122 'z' */
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, /* 123 '{' */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* 124 '|' */
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, /* 125 '}' */
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}  /* 126 '~' */
};

/* =============================================================================
 * 2D Graphics Drawing Toolkit
 * ============================================================================= */
static inline void os_fill_rect(os_window_t *win, int x, int y, int w, int h, uint32_t color) {
    if (!win || !win->canvas || w <= 0 || h <= 0) return;
    int cw = win->client_w;
    int ch = win->client_h;

    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = x + w > cw ? cw : x + w;
    int y2 = y + h > ch ? ch : y + h;
    if (x1 >= x2 || y1 >= y2) return;

    for (int cy = y1; cy < y2; cy++) {
        uint32_t *row = win->canvas + cy * cw + x1;
        for (int cx = x1; cx < x2; cx++) {
            *row++ = color;
        }
    }
}

static inline void os_draw_rect(os_window_t *win, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    os_fill_rect(win, x, y, w, 1, color);
    os_fill_rect(win, x, y + h - 1, w, 1, color);
    os_fill_rect(win, x, y, 1, h, color);
    os_fill_rect(win, x + w - 1, y, 1, h, color);
}

static inline void os_draw_char(os_window_t *win, int x, int y, char c, uint32_t color) {
    if (!win || !win->canvas) return;
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *glyph = os_font8x8[c - 32];
    int cw = win->client_w;
    int ch = win->client_h;

    for (int row = 0; row < 8; row++) {
        int py = y + row;
        if (py < 0 || py >= ch) continue;
        uint8_t bits = glyph[row];
        uint32_t *prow = win->canvas + py * cw;
        for (int col = 0; col < 8; col++) {
            int px = x + col;
            if (px < 0 || px >= cw) continue;
            if (bits & (0x80 >> col)) {
                prow[px] = color;
            }
        }
    }
}

static inline void os_draw_text(os_window_t *win, int x, int y, const char *str, uint32_t color) {
    if (!win || !win->canvas || !str) return;
    int cur_x = x;
    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            y += 10;
        } else {
            os_draw_char(win, cur_x, y, *str, color);
            cur_x += 8;
        }
        str++;
    }
}

static inline void os_draw_circle(os_window_t *win, int cx, int cy, int radius, uint32_t color) {
    if (!win || !win->canvas || radius <= 0) return;
    int r2 = radius * radius;
    int cw = win->client_w;
    int ch = win->client_h;

    for (int dy = -radius; dy <= radius; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= ch) continue;
        uint32_t *prow = win->canvas + py * cw;
        for (int dx = -radius; dx <= radius; dx++) {
            int px = cx + dx;
            if (px < 0 || px >= cw) continue;
            if (dx*dx + dy*dy <= r2) {
                prow[px] = color;
            }
        }
    }
}

static inline bool os_is_inside(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

static inline void os_draw_button(os_window_t *win, int x, int y, int w, int h, const char *text, uint32_t bg, uint32_t fg, int is_pressed) {
    if (is_pressed) {
        /* Pressed state: slightly offset/darkened */
        os_fill_rect(win, x, y, w, h, bg);
        os_draw_rect(win, x, y, w, h, OS_COLOR_SLATE_400);
        int text_len = (int)os_strlen(text);
        int tx = x + (w - text_len * 8) / 2 + 1;
        int ty = y + (h - 8) / 2 + 1;
        os_draw_text(win, tx, ty, text, fg);
    } else {
        /* Normal state: top highlight line, card body, subtle border */
        os_fill_rect(win, x, y, w, h, bg);
        os_draw_rect(win, x, y, w, h, OS_COLOR_SLATE_700);
        os_fill_rect(win, x + 1, y + 1, w - 2, 1, OS_ARGB(0x40, 0xFF, 0xFF, 0xFF));
        int text_len = (int)os_strlen(text);
        int tx = x + (w - text_len * 8) / 2;
        int ty = y + (h - 8) / 2;
        os_draw_text(win, tx, ty, text, fg);
    }
}

/* =============================================================================
 * Advanced Modern UI Engine: Gradients, Rounded Rects, Glass Cards & Widgets
 * ============================================================================= */

static inline uint32_t os_alpha_blend(uint32_t src, uint32_t dst) {
    uint32_t a = (src >> 24) & 0xFF;
    if (a == 0) return dst;
    if (a == 255) return src;
    uint32_t inv_a = 255 - a;

    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;

    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;

    uint32_t rr = (sr * a + dr * inv_a) / 255;
    uint32_t rg = (sg * a + dg * inv_a) / 255;
    uint32_t rb = (sb * a + db * inv_a) / 255;

    return 0xFF000000 | (rr << 16) | (rg << 8) | rb;
}

static inline void os_put_pixel_blend(os_window_t *win, int x, int y, uint32_t color) {
    if (!win || !win->canvas) return;
    if (x < 0 || x >= win->client_w || y < 0 || y >= win->client_h) return;
    uint32_t *p = win->canvas + y * win->client_w + x;
    *p = os_alpha_blend(color, *p);
}

/* Vertical gradient fill */
static inline void os_fill_gradient_v(os_window_t *win, int x, int y, int w, int h, uint32_t c1, uint32_t c2) {
    if (!win || !win->canvas || w <= 0 || h <= 0) return;
    int cw = win->client_w;
    int ch = win->client_h;

    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = x + w > cw ? cw : x + w;
    int y2 = y + h > ch ? ch : y + h;
    if (x1 >= x2 || y1 >= y2) return;

    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF, a1 = (c1 >> 24) & 0xFF;
    int r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF, a2 = (c2 >> 24) & 0xFF;

    for (int cy = y1; cy < y2; cy++) {
        int t = ((cy - y) * 255) / (h > 1 ? h - 1 : 1);
        uint32_t r = r1 + ((r2 - r1) * t) / 255;
        uint32_t g = g1 + ((g2 - g1) * t) / 255;
        uint32_t b = b1 + ((b2 - b1) * t) / 255;
        uint32_t a = a1 + ((a2 - a1) * t) / 255;
        uint32_t col = OS_ARGB(a, r, g, b);

        uint32_t *row = win->canvas + cy * cw + x1;
        for (int cx = x1; cx < x2; cx++) {
            *row++ = col;
        }
    }
}

/* Fast integer square root for subpixel circle / rounded rect distance */
static inline uint32_t os_isqrt(uint32_t n) {
    uint32_t root = 0, bit = 1U << 30;
    while (bit > n) bit >>= 2;
    while (bit != 0) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

/* Smooth Rounded Rectangle Fill with Subpixel Anti-Aliasing */
static inline void os_fill_rounded_rect(os_window_t *win, int x, int y, int w, int h, int r, uint32_t color) {
    if (!win || !win->canvas || w <= 0 || h <= 0) return;
    if (r <= 0) {
        os_fill_rect(win, x, y, w, h, color);
        return;
    }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    int cw = win->client_w;
    int ch = win->client_h;

    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = x + w > cw ? cw : x + w;
    int y2 = y + h > ch ? ch : y + h;
    if (x1 >= x2 || y1 >= y2) return;

    uint32_t base_alpha = (color >> 24) & 0xFF;
    if (base_alpha == 0) return;
    uint32_t rgb = color & 0x00FFFFFF;
    int r_fp = r << 8;

    for (int cy = y1; cy < y2; cy++) {
        uint32_t *row = win->canvas + cy * cw + x1;
        for (int cx = x1; cx < x2; cx++) {
            int dx = 0, dy = 0;
            if (cx < x + r && cy < y + r) {
                dx = (x + r) - cx; dy = (y + r) - cy;
            } else if (cx >= x + w - r && cy < y + r) {
                dx = cx - (x + w - r - 1); dy = (y + r) - cy;
            } else if (cx < x + r && cy >= y + h - r) {
                dx = (x + r) - cx; dy = cy - (y + h - r - 1);
            } else if (cx >= x + w - r && cy >= y + h - r) {
                dx = cx - (x + w - r - 1); dy = cy - (y + h - r - 1);
            }

            if (dx > 0 && dy > 0) {
                int dist2 = dx * dx + dy * dy;
                int dist_fp = (int)os_isqrt((uint32_t)(dist2 << 16));
                int cov = r_fp + 128 - dist_fp;
                if (cov <= 0) {
                    row++;
                    continue;
                }
                if (cov < 256) {
                    uint32_t a = (base_alpha * (uint32_t)cov) >> 8;
                    *row = os_alpha_blend((a << 24) | rgb, *row);
                    row++;
                    continue;
                }
            }

            if (base_alpha == 255) {
                *row++ = color;
            } else {
                *row = os_alpha_blend(color, *row);
                row++;
            }
        }
    }
}

/* Smooth Rounded Rectangle Outline with Subpixel Anti-Aliasing */
static inline void os_draw_rounded_rect(os_window_t *win, int x, int y, int w, int h, int r, uint32_t color) {
    if (!win || !win->canvas || w <= 0 || h <= 0) return;
    if (r <= 0) {
        os_draw_rect(win, x, y, w, h, color);
        return;
    }
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;

    os_fill_rect(win, x + r, y, w - 2 * r, 1, color);
    os_fill_rect(win, x + r, y + h - 1, w - 2 * r, 1, color);
    os_fill_rect(win, x, y + r, 1, h - 2 * r, color);
    os_fill_rect(win, x + w - 1, y + r, 1, h - 2 * r, color);

    uint32_t base_alpha = (color >> 24) & 0xFF;
    uint32_t rgb = color & 0x00FFFFFF;
    int r_outer_fp = r << 8;
    int r_inner_fp = (r > 1 ? (r - 1) : 0) << 8;

    for (int dy = 0; dy <= r; dy++) {
        for (int dx = 0; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            int d_fp = (int)os_isqrt((uint32_t)(d2 << 16));
            int cov_out = r_outer_fp + 128 - d_fp;
            int cov_in  = d_fp - (r_inner_fp - 128);
            int cov = cov_out < cov_in ? cov_out : cov_in;
            if (cov <= 0) continue;
            if (cov > 255) cov = 255;
            uint32_t a = (base_alpha * (uint32_t)cov) >> 8;
            if (a == 0) continue;
            uint32_t c = (a << 24) | rgb;

            os_put_pixel_blend(win, x + r - dx, y + r - dy, c);
            os_put_pixel_blend(win, x + w - 1 - r + dx, y + r - dy, c);
            os_put_pixel_blend(win, x + r - dx, y + h - 1 - r + dy, c);
            os_put_pixel_blend(win, x + w - 1 - r + dx, y + h - 1 - r + dy, c);
        }
    }
}

/* =============================================================================
 * Anti-Aliased Typography Engine (Segoe UI 16px & Consolas 15px)
 * ============================================================================= */

static inline void os_draw_char_ui_aa(os_window_t *win, int x, int y, char c, uint32_t color) {
    if (!win || !win->canvas) return;
    if (c < 32 || c > 126) c = ' ';
    int idx = c - 32;
    const uint8_t *glyph = os_font_ui_data[idx];
    int cw = win->client_w;
    int ch = win->client_h;
    uint32_t base_alpha = (color >> 24) & 0xFF;
    if (base_alpha == 0) return;
    uint32_t rgb = color & 0x00FFFFFF;

    for (int row = 0; row < OS_FONT_UI_H; row++) {
        int py = y + row;
        if (py < 0 || py >= ch) continue;
        uint32_t *prow = win->canvas + py * cw;
        const uint8_t *grow = glyph + row * OS_FONT_UI_MAX_W;
        for (int col = 0; col < OS_FONT_UI_MAX_W; col++) {
            uint32_t galpha = grow[col];
            if (galpha == 0) continue;
            int px = x + col;
            if (px < 0 || px >= cw) continue;

            uint32_t final_alpha = (base_alpha * galpha) >> 8;
            if (final_alpha == 0) continue;

            prow[px] = os_alpha_blend((final_alpha << 24) | rgb, prow[px]);
        }
    }
}

static inline int os_text_width_ui_aa(const char *str) {
    if (!str) return 0;
    int w = 0;
    while (*str) {
        if (*str == '\n') break;
        char c = *str;
        if (c >= 32 && c <= 126) {
            w += os_font_ui_widths[c - 32];
        } else {
            w += 6;
        }
        str++;
    }
    return w;
}

static inline int os_draw_text_ui_aa(os_window_t *win, int x, int y, const char *str, uint32_t color) {
    if (!win || !win->canvas || !str) return 0;
    int cur_x = x;
    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            y += 20;
        } else {
            char c = *str;
            if (c >= 32 && c <= 126) {
                os_draw_char_ui_aa(win, cur_x, y, c, color);
                cur_x += os_font_ui_widths[c - 32];
            } else {
                cur_x += 6;
            }
        }
        str++;
    }
    return cur_x - x;
}

static inline void os_draw_char_mono_aa(os_window_t *win, int x, int y, char c, uint32_t color) {
    if (!win || !win->canvas) return;
    if (c < 32 || c > 126) c = ' ';
    int idx = c - 32;
    const uint8_t *glyph = os_font_mono_data[idx];
    int cw = win->client_w;
    int ch = win->client_h;
    uint32_t base_alpha = (color >> 24) & 0xFF;
    if (base_alpha == 0) return;
    uint32_t rgb = color & 0x00FFFFFF;

    for (int row = 0; row < OS_FONT_MONO_H; row++) {
        int py = y + row;
        if (py < 0 || py >= ch) continue;
        uint32_t *prow = win->canvas + py * cw;
        const uint8_t *grow = glyph + row * OS_FONT_MONO_W;
        for (int col = 0; col < OS_FONT_MONO_W; col++) {
            uint32_t galpha = grow[col];
            if (galpha == 0) continue;
            int px = x + col;
            if (px < 0 || px >= cw) continue;

            uint32_t final_alpha = (base_alpha * galpha) >> 8;
            if (final_alpha == 0) continue;

            prow[px] = os_alpha_blend((final_alpha << 24) | rgb, prow[px]);
        }
    }
}

static inline int os_draw_text_mono_aa(os_window_t *win, int x, int y, const char *str, uint32_t color) {
    if (!win || !win->canvas || !str) return 0;
    int cur_x = x;
    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            y += 20;
        } else {
            char c = *str;
            if (c >= 32 && c <= 126) {
                os_draw_char_mono_aa(win, cur_x, y, c, color);
            }
            cur_x += OS_FONT_MONO_W;
        }
        str++;
    }
    return cur_x - x;
}

/* Bilinear smooth 2x scaling for large digits / display (16x30) */
static inline void os_draw_char_mono_aa_2x(os_window_t *win, int x, int y, char c, uint32_t color) {
    if (!win || !win->canvas) return;
    if (c < 32 || c > 126) c = ' ';
    int idx = c - 32;
    const uint8_t *glyph = os_font_mono_data[idx];
    int cw = win->client_w;
    int ch = win->client_h;
    uint32_t base_alpha = (color >> 24) & 0xFF;
    if (base_alpha == 0) return;
    uint32_t rgb = color & 0x00FFFFFF;

    for (int py = 0; py < 30; py++) {
        int vy = y + py;
        if (vy < 0 || vy >= ch) continue;
        uint32_t *prow = win->canvas + vy * cw;

        int gy = py >> 1;
        int fy = (py & 1) ? 128 : 0;
        int gy2 = (gy + 1 < OS_FONT_MONO_H) ? gy + 1 : gy;

        for (int px = 0; px < 16; px++) {
            int vx = x + px;
            if (vx < 0 || vx >= cw) continue;

            int gx = px >> 1;
            int fx = (px & 1) ? 128 : 0;
            int gx2 = (gx + 1 < OS_FONT_MONO_W) ? gx + 1 : gx;

            uint32_t a00 = glyph[gy * OS_FONT_MONO_W + gx];
            uint32_t a10 = glyph[gy * OS_FONT_MONO_W + gx2];
            uint32_t a01 = glyph[gy2 * OS_FONT_MONO_W + gx];
            uint32_t a11 = glyph[gy2 * OS_FONT_MONO_W + gx2];

            uint32_t top = a00 * (256 - fx) + a10 * fx;
            uint32_t bot = a01 * (256 - fx) + a11 * fx;
            uint32_t galpha = (top * (256 - fy) + bot * fy) >> 16;

            if (galpha < 8) continue;
            uint32_t final_alpha = (base_alpha * galpha) >> 8;
            if (final_alpha == 0) continue;

            prow[vx] = os_alpha_blend((final_alpha << 24) | rgb, prow[vx]);
        }
    }
}

static inline int os_draw_text_mono_aa_2x(os_window_t *win, int x, int y, const char *str, uint32_t color) {
    if (!win || !win->canvas || !str) return 0;
    int cur_x = x;
    while (*str) {
        char c = *str;
        if (c >= 32 && c <= 126) {
            os_draw_char_mono_aa_2x(win, cur_x, y, c, color);
        }
        cur_x += 16;
        str++;
    }
    return cur_x - x;
}

/* Modern Vector Icons (Anti-Aliased Squircles & Soft Highlights) */
static inline void os_draw_icon_folder(os_window_t *win, int x, int y) {
    /* Back folder tab with subtle rounded curve */
    os_fill_rounded_rect(win, x, y + 1, 11, 6, 2, OS_ARGB(0xFF, 0x02, 0x84, 0xC7));
    /* Front folder body with rich sky-blue tone */
    os_fill_rounded_rect(win, x, y + 5, 22, 13, 3, OS_ARGB(0xFF, 0x38, 0xBD, 0xF8));
    /* Soft top highlight rim */
    os_fill_rounded_rect(win, x + 1, y + 6, 20, 2, 1, OS_ARGB(0x70, 0xFF, 0xFF, 0xFF));
    /* Subtle inner surface depth */
    os_fill_rounded_rect(win, x + 2, y + 9, 18, 8, 2, OS_ARGB(0x18, 0x02, 0x84, 0xC7));
    /* Anti-aliased outer border */
    os_draw_rounded_rect(win, x, y + 5, 22, 13, 3, OS_ARGB(0x30, 0xFF, 0xFF, 0xFF));
}

static inline void os_draw_icon_file(os_window_t *win, int x, int y, int is_elf) {
    uint32_t bg = is_elf ? OS_ARGB(0xFF, 0x63, 0x66, 0xF1) : OS_ARGB(0xFF, 0x47, 0x55, 0x69);
    uint32_t fg = is_elf ? OS_COLOR_CYAN_NEON : OS_COLOR_SLATE_200;
    /* File document card */
    os_fill_rounded_rect(win, x, y, 18, 20, 3, bg);
    os_draw_rounded_rect(win, x, y, 18, 20, 3, OS_ARGB(0x40, 0xFF, 0xFF, 0xFF));
    /* Top-right folded dog-ear */
    os_fill_rounded_rect(win, x + 10, y, 8, 7, 1, OS_ARGB(0x35, 0x00, 0x00, 0x00));
    os_fill_rounded_rect(win, x + 11, y, 7, 6, 1, OS_ARGB(0x40, 0xFF, 0xFF, 0xFF));
    /* Anti-aliased content lines */
    os_fill_rounded_rect(win, x + 3, y + 8, 11, 2, 1, fg);
    os_fill_rounded_rect(win, x + 3, y + 12, 9, 2, 1, fg);
    os_fill_rounded_rect(win, x + 3, y + 15, 6, 2, 1, OS_ARGB(0x80, 0xFF, 0xFF, 0xFF));
}

/* Elevated Acrylic Glass Card with specular rim */
static inline void os_draw_card(os_window_t *win, int x, int y, int w, int h, uint32_t bg, uint32_t border, int r) {
    os_fill_rounded_rect(win, x, y, w, h, r, bg);
    os_draw_rounded_rect(win, x, y, w, h, r, border);
    if (w > 2 * r + 4) {
        os_fill_rect(win, x + r + 2, y + 1, w - 2 * r - 4, 1, OS_ARGB(0x35, 0xFF, 0xFF, 0xFF));
    }
}

/* Modern pill or card button with interactive hover/pressed states and Segoe UI typography */
static inline void os_draw_button_modern(os_window_t *win, int x, int y, int w, int h, const char *text, uint32_t accent, int is_hovered, int is_pressed) {
    uint32_t bg = is_pressed ? OS_COLOR_SLATE_950 : (is_hovered ? OS_COLOR_SLATE_700 : OS_COLOR_SLATE_800);
    uint32_t border = is_hovered ? accent : OS_COLOR_SLATE_600;
    os_draw_card(win, x, y, w, h, bg, border, 6);
    int tw = os_text_width_ui_aa(text);
    int tx = x + (w - tw) / 2 + (is_pressed ? 1 : 0);
    int ty = y + (h - OS_FONT_UI_H) / 2 + (is_pressed ? 1 : 0);
    uint32_t fg = is_pressed ? OS_COLOR_SLATE_400 : (is_hovered ? OS_COLOR_WHITE : OS_COLOR_SLATE_200);
    os_draw_text_ui_aa(win, tx, ty, text, fg);
}

/* Category or Type Badge with Segoe UI typography */
static inline void os_draw_badge(os_window_t *win, int x, int y, const char *text, uint32_t bg, uint32_t fg) {
    int tw = os_text_width_ui_aa(text);
    int bw = tw + 16;
    int bh = 20;
    os_fill_rounded_rect(win, x, y, bw, bh, 5, bg);
    os_draw_rounded_rect(win, x, y, bw, bh, 5, OS_ARGB(0x40, 0xFF, 0xFF, 0xFF));
    os_draw_text_ui_aa(win, x + 8, y + 2, text, fg);
}

/* Modern Minimalist Scrollbar */
static inline void os_draw_scrollbar(os_window_t *win, int x, int y, int h, int total, int visible, int offset) {
    if (total <= visible || total <= 0) return;
    os_fill_rounded_rect(win, x, y, 6, h, 3, OS_COLOR_SLATE_950);
    int thumb_h = (visible * h) / total;
    if (thumb_h < 16) thumb_h = 16;
    int max_scroll = total - visible;
    int thumb_y = y + (offset * (h - thumb_h)) / (max_scroll > 0 ? max_scroll : 1);
    os_fill_rounded_rect(win, x + 1, thumb_y, 4, thumb_h, 2, OS_COLOR_SLATE_500);
}

#endif /* OPENSWEET_H */
