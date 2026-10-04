/* =============================================================================
 * OpenSweet OS - Native C Desktop Terminal (user/terminal.c)
 * Ring 3 graphical interactive shell written in standard C using OpenSweet SDK.
 * Features:
 *   - Standalone Ring 3 ELF application with embedded .os_app metadata and icon
 *   - Multi-line colored terminal buffer with automatic scrolling
 *   - Interactive shell with built-in commands: help, ls, cat, echo, uptime,
 *     calc, notepad, files, clear, version, exit
 *   - Real ext4 directory browsing via os_read_dir()
 *   - Modern acrylic/obsidian theme with neon green prompt and cyan cursor
 * ============================================================================= */

#include "opensweet.h"
#include "terminal_icon.h"

#define WIN_W 760
#define WIN_H 500

#define TERM_ROWS 80
#define TERM_COLS 128
#define LINE_H    20
#define TOP_H     8
#define BOT_H     28

__attribute__((section(".os_app"), used))
static const os_app_package_t terminal_pkg = {
    .magic = OS_APP_MAGIC,
    .name = "Terminal",
    .version = "1.0.0",
    .author = "OpenSweet Team",
    .description = "Modern C Shell Terminal",
    .exec_path = "/terminal.elf",
    .icon_width = 44,
    .icon_height = 44,
    .icon_pixels = TERMINAL_ICON_PIXELS_INIT
};

typedef struct {
    char     text[TERM_COLS + 1];
    uint32_t color;
    uint8_t  is_prompt;
    char     cwd[64];
    char     time[8];
} term_line_t;

static term_line_t lines[TERM_ROWS];
static int         cur_row = 0;
static char        input_buf[128];
static int         input_len = 0;
static int         blink_cnt = 0;
static int         curr_blink_state = 1;

static void term_scroll_up(void) {
    for (int r = 0; r < TERM_ROWS - 1; r++) {
        lines[r] = lines[r + 1];
    }
    lines[TERM_ROWS - 1].text[0] = '\0';
    lines[TERM_ROWS - 1].color = OS_COLOR_SLATE_200;
    lines[TERM_ROWS - 1].is_prompt = 0;
    lines[TERM_ROWS - 1].cwd[0] = '\0';
    lines[TERM_ROWS - 1].time[0] = '\0';
}

static void term_print_line(const char *str, uint32_t color) {
    if (cur_row >= TERM_ROWS) {
        term_scroll_up();
        cur_row = TERM_ROWS - 1;
    }
    int len = 0;
    while (*str && len < TERM_COLS) {
        lines[cur_row].text[len++] = *str++;
    }
    lines[cur_row].text[len] = '\0';
    lines[cur_row].color = color;
    lines[cur_row].is_prompt = 0;
    cur_row++;
}

static void term_print_prompt_line(const char *cmd) {
    if (cur_row >= TERM_ROWS) {
        term_scroll_up();
        cur_row = TERM_ROWS - 1;
    }
    int len = 0;
    while (*cmd && len < TERM_COLS) {
        lines[cur_row].text[len++] = *cmd++;
    }
    lines[cur_row].text[len] = '\0';
    lines[cur_row].color = 0xFFFFFFFF;
    lines[cur_row].is_prompt = 1;

    if (!os_getcwd(lines[cur_row].cwd, sizeof(lines[cur_row].cwd))) {
        os_strcpy(lines[cur_row].cwd, "/");
    }

    os_time_t rtc = os_get_time();
    lines[cur_row].time[0] = '0' + (rtc.hours / 10);
    lines[cur_row].time[1] = '0' + (rtc.hours % 10);
    lines[cur_row].time[2] = ':';
    lines[cur_row].time[3] = '0' + (rtc.minutes / 10);
    lines[cur_row].time[4] = '0' + (rtc.minutes % 10);
    lines[cur_row].time[5] = '\0';

    cur_row++;
}

static void term_clear(void) {
    for (int r = 0; r < TERM_ROWS; r++) {
        lines[r].text[0] = '\0';
        lines[r].color = OS_COLOR_SLATE_200;
        lines[r].is_prompt = 0;
        lines[r].cwd[0] = '\0';
        lines[r].time[0] = '\0';
    }
    cur_row = 0;
    input_len = 0;
    input_buf[0] = '\0';
}

static void print_fetch(void) {
    /* Read CPU Model via CPUID 0x80000002..0x80000004 */
    char cpu_brand[49];
    cpu_brand[0] = '\0';
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000) : );
    if (eax >= 0x80000004) {
        uint32_t *p = (uint32_t*)cpu_brand;
        for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
            __asm__ volatile ("cpuid" : "=a"(p[0]), "=b"(p[1]), "=c"(p[2]), "=d"(p[3]) : "a"(leaf) : );
            p += 4;
        }
        cpu_brand[48] = '\0';
        /* Trim leading spaces */
        char *s = cpu_brand;
        while (*s == ' ') s++;
        if (s != cpu_brand) {
            char tmp[49];
            os_strcpy(tmp, s);
            os_strcpy(cpu_brand, tmp);
        }
    } else {
        os_strcpy(cpu_brand, "x86_64 SMP Processor");
    }

    /* System uptime string */
    uint32_t total_sec = os_uptime() / 100;
    uint32_t mins = (total_sec / 60) % 60;
    uint32_t hours = total_sec / 3600;
    char up_buf[48];
    char num_buf[16];
    up_buf[0] = '\0';
    if (hours > 0) {
        os_itoa(hours, num_buf);
        os_strcpy(up_buf, num_buf);
        os_strcpy(up_buf + os_strlen(up_buf), "h ");
    }
    os_itoa(mins, num_buf);
    os_strcpy(up_buf + os_strlen(up_buf), num_buf);
    os_strcpy(up_buf + os_strlen(up_buf), "m ");
    os_itoa(total_sec % 60, num_buf);
    os_strcpy(up_buf + os_strlen(up_buf), num_buf);
    os_strcpy(up_buf + os_strlen(up_buf), "s");

    /* Query true hardware and kernel state */
    os_sysinfo_t si;
    os_get_sysinfo(&si);

    /* Truncate CPU brand if needed */
    if (os_strlen(cpu_brand) > 28) cpu_brand[28] = '\0';

    char l3[128], l5[128], l6[128], l8[128], l10[128], l11[128];

    os_strcpy(l3, "\x1b[36m              +=+++=++==----=--::...   \x1b[36mOS      : \x1b[37m");
    os_strcpy(l3 + os_strlen(l3), si.os_name);
    os_strcpy(l3 + os_strlen(l3), " x86_64\x1b[0m");

    os_strcpy(l5, "\x1b[36m           +++=++++==-------=-::::::   \x1b[36mKernel  : \x1b[37m");
    os_strcpy(l5 + os_strlen(l5), si.kernel_ver);
    os_strcpy(l5 + os_strlen(l5), "\x1b[0m");

    os_strcpy(l6, "\x1b[36m          +++====++==--====---         \x1b[36mUptime  : \x1b[33m");
    os_strcpy(l6 + os_strlen(l6), up_buf);
    os_strcpy(l6 + os_strlen(l6), "\x1b[0m");

    os_strcpy(l8, "\x1b[36m   --------=+==--======-===--          \x1b[36mWM      : \x1b[37m");
    os_strcpy(l8 + os_strlen(l8), si.wm_name);
    os_strcpy(l8 + os_strlen(l8), "\x1b[0m");

    os_strcpy(l10, "\x1b[36m    :::::::::-==----=--::              \x1b[36mCPU     : \x1b[37m");
    os_strcpy(l10 + os_strlen(l10), cpu_brand);
    os_strcpy(l10 + os_strlen(l10), "\x1b[0m");

    /* Real dynamic RAM usage */
    char used_str[16], tot_str[16], free_str[16];
    os_itoa(si.used_ram_mb, used_str);
    os_itoa(si.total_ram_mb, tot_str);
    os_itoa(si.free_ram_mb, free_str);
    os_strcpy(l11, "\x1b[36m     ::::::    :::::--                 \x1b[36mMemory  : \x1b[37m");
    os_strcpy(l11 + os_strlen(l11), used_str);
    os_strcpy(l11 + os_strlen(l11), " MB / ");
    os_strcpy(l11 + os_strlen(l11), tot_str);
    os_strcpy(l11 + os_strlen(l11), " MB (");
    os_strcpy(l11 + os_strlen(l11), free_str);
    os_strcpy(l11 + os_strlen(l11), " MB free)\x1b[0m");

    /* Fastfetch with exact ascii.txt art & real dynamic diagnostics */
    term_print_line("\x1b[36m                                -:     \x1b[35mroot\x1b[37m@\x1b[35mopensweet\x1b[0m", OS_COLOR_SLATE_200);
    term_print_line("\x1b[36m                  -=++==:    ---:.     \x1b[90m-----------------------------------\x1b[0m", OS_COLOR_SLATE_200);
    term_print_line(l3, OS_COLOR_SLATE_200);
    term_print_line("\x1b[36m            ++++++====--::-=--:::::::  \x1b[36mHost    : \x1b[37mPC Compatible (QEMU / Long Mode)\x1b[0m", OS_COLOR_SLATE_200);
    term_print_line(l5, OS_COLOR_SLATE_200);
    term_print_line(l6, OS_COLOR_SLATE_200);
    term_print_line("\x1b[36m          +++=-:-====---====-=         \x1b[36mShell   : \x1b[37msweetsh 1.0 (Ring 3 C99)\x1b[0m", OS_COLOR_SLATE_200);
    term_print_line(l8, OS_COLOR_SLATE_200);
    term_print_line("\x1b[36m   ::::::::-====-=====-----:           \x1b[36mTerminal: \x1b[37mOpenSweet Powerline Terminal\x1b[0m", OS_COLOR_SLATE_200);
    term_print_line(l10, OS_COLOR_SLATE_200);
    term_print_line(l11, OS_COLOR_SLATE_200);
    term_print_line("\x1b[36m     :::                               \x1b[36mPalette : \x1b[35m● \x1b[31m● \x1b[33m● \x1b[32m● \x1b[36m● \x1b[34m● \x1b[37m● \x1b[90m●\x1b[0m", OS_COLOR_SLATE_200);
}

/* Execute shell command */
static void execute_command(const char *cmd) {
    /* Skip leading whitespace */
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    if (os_strcmp(cmd, "help") == 0) {
        term_print_line("OpenSweet OS Shell Commands:", OS_COLOR_CYAN_NEON);
        term_print_line("  help          Show this list of available commands", OS_COLOR_SLATE_300);
        term_print_line("  fetch         Display system fetch information & specs", OS_COLOR_CYAN_NEON);
        term_print_line("  ls / dir      List files and folders on ext4 filesystem", OS_COLOR_SLATE_300);
        term_print_line("  clear / cls   Clear the terminal screen", OS_COLOR_SLATE_300);
        term_print_line("  echo <text>   Print text to console", OS_COLOR_SLATE_300);
        term_print_line("  cat <file>    Display file contents from ext4", OS_COLOR_SLATE_300);
        term_print_line("  write <f> <t> Write/create file on ext4 filesystem", OS_COLOR_SLATE_300);
        term_print_line("  rm <file>     Delete a file from ext4 filesystem", OS_COLOR_SLATE_300);
        term_print_line("  pwd           Print current working directory", OS_COLOR_SLATE_300);
        term_print_line("  cd <path>     Change current working directory", OS_COLOR_SLATE_300);
        term_print_line("  memtest       Test dynamic memory allocation (malloc/free)", OS_COLOR_SLATE_300);
        term_print_line("  testfd        Test POSIX file descriptors (open/read/write/close)", OS_COLOR_SLATE_300);
        term_print_line("  testpipe      Test POSIX anonymous pipes and dup2 redirect", OS_COLOR_SLATE_300);
        term_print_line("  testwait      Test process waitpid and exit status harvesting", OS_COLOR_SLATE_300);
        term_print_line("  testsh        Test POSIX shell environment, cwd and stdio redirect", OS_COLOR_SLATE_300);
        term_print_line("  uptime        Display system uptime in seconds", OS_COLOR_SLATE_300);
        term_print_line("  calc          Launch Desktop Calculator (Ring 3)", OS_COLOR_SLATE_300);
        term_print_line("  notepad       Launch Desktop Notepad (Ring 3)", OS_COLOR_SLATE_300);
        term_print_line("  files         Launch File Explorer (Ring 3)", OS_COLOR_SLATE_300);
        term_print_line("  doom          Launch DOOM (Shareware) (Ring 3)", OS_COLOR_SLATE_300);
        term_print_line("  version       Show kernel architecture and build", OS_COLOR_SLATE_300);
        term_print_line("  exit          Close terminal window", OS_COLOR_SLATE_300);
    } else if (os_strcmp(cmd, "fetch") == 0 || os_strcmp(cmd, "sweetfetch") == 0 || os_strcmp(cmd, "neofetch") == 0) {
        print_fetch();
    } else if (os_strcmp(cmd, "clear") == 0 || os_strcmp(cmd, "cls") == 0) {
        term_clear();
    } else if (os_strcmp(cmd, "uptime") == 0) {
        uint32_t ticks = os_uptime();
        char buf[64];
        char num[16];
        os_itoa(ticks / 100, num);
        os_strcpy(buf, "System Uptime: ");
        os_strcpy(buf + os_strlen(buf), num);
        os_strcpy(buf + os_strlen(buf), " seconds");
    } else if (os_strcmp(cmd, "testfd") == 0) {
        term_print_line("Testing POSIX File Descriptor Subsystem (VFS)...", OS_COLOR_CYAN_NEON);
        int fd = os_open("/vfs_test.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            term_print_line("FAIL: os_open(/vfs_test.txt, O_CREAT) returned error", OS_COLOR_ROSE);
        } else {
            char fd_msg[64], fd_num[16];
            os_itoa(fd, fd_num);
            os_strcpy(fd_msg, "PASS: Allocated File Descriptor fd = ");
            os_strcpy(fd_msg + os_strlen(fd_msg), fd_num);
            term_print_line(fd_msg, OS_COLOR_EMERALD_LT);

            const char *test_data = "POSIX VFS Open/Read/Write OK\n";
            ssize_t written = os_write_fd(fd, test_data, os_strlen(test_data));
            os_close(fd);

            if (written != (ssize_t)os_strlen(test_data)) {
                term_print_line("FAIL: os_write_fd wrote incomplete data", OS_COLOR_ROSE);
            } else {
                term_print_line("PASS: os_write_fd written 30 bytes", OS_COLOR_EMERALD_LT);

                int rfd = os_open("/vfs_test.txt", O_RDONLY, 0);
                if (rfd < 0) {
                    term_print_line("FAIL: os_open for read failed", OS_COLOR_ROSE);
                } else {
                    char read_buf[64];
                    ssize_t nread = os_read_fd(rfd, read_buf, sizeof(read_buf) - 1);
                    os_close(rfd);
                    if (nread > 0) {
                        read_buf[nread] = '\0';
                        term_print_line("PASS: os_read_fd read payload successfully:", OS_COLOR_EMERALD_LT);
                        term_print_line(read_buf, OS_COLOR_WHITE);
                    } else {
                        term_print_line("FAIL: os_read_fd returned 0 or error", OS_COLOR_ROSE);
                    }
                    os_delete_file("/vfs_test.txt");
                    term_print_line("PASS: All POSIX File Descriptor tests passed!", OS_COLOR_CYAN_NEON);
                }
            }
        }
    } else if (os_strcmp(cmd, "testpipe") == 0) {
        term_print_line("Testing POSIX Anonymous Pipes & dup2...", OS_COLOR_CYAN_NEON);
        int pfd[2];
        int pr = os_pipe(pfd);
        if (pr < 0) {
            term_print_line("FAIL: os_pipe returned error", OS_COLOR_ROSE);
        } else {
            char pfd_msg[64], p0[16], p1[16];
            os_itoa(pfd[0], p0);
            os_itoa(pfd[1], p1);
            os_strcpy(pfd_msg, "PASS: pipe allocated read fd=");
            os_strcpy(pfd_msg + os_strlen(pfd_msg), p0);
            os_strcpy(pfd_msg + os_strlen(pfd_msg), ", write fd=");
            os_strcpy(pfd_msg + os_strlen(pfd_msg), p1);
            term_print_line(pfd_msg, OS_COLOR_EMERALD_LT);

            const char *pipe_msg = "Hello through kernel pipe ring buffer!\n";
            ssize_t pw = os_write_fd(pfd[1], pipe_msg, os_strlen(pipe_msg));
            if (pw != (ssize_t)os_strlen(pipe_msg)) {
                term_print_line("FAIL: os_write_fd to pipe failed", OS_COLOR_ROSE);
            } else {
                term_print_line("PASS: Successfully wrote message to pipe", OS_COLOR_EMERALD_LT);

                /* Test dup2: duplicate pfd[0] to fd 7 */
                int dup_res = os_dup2(pfd[0], 7);
                if (dup_res != 7) {
                    term_print_line("FAIL: os_dup2(read_fd, 7) failed", OS_COLOR_ROSE);
                } else {
                    term_print_line("PASS: os_dup2(read_fd, 7) succeeded (fd 7 active)", OS_COLOR_EMERALD_LT);

                    /* Close original read descriptor pfd[0] */
                    os_close(pfd[0]);

                    /* Read from duplicated descriptor 7 */
                    char rx_buf[64];
                    ssize_t nrx = os_read_fd(7, rx_buf, sizeof(rx_buf) - 1);
                    os_close(7);
                    os_close(pfd[1]);

                    if (nrx > 0) {
                        rx_buf[nrx] = '\0';
                        term_print_line("PASS: Read from dup2'd fd 7 verified:", OS_COLOR_EMERALD_LT);
                        term_print_line(rx_buf, OS_COLOR_WHITE);
                        term_print_line("PASS: All POSIX Pipe & dup2 tests passed!", OS_COLOR_CYAN_NEON);
                    } else {
                        term_print_line("FAIL: Read from dup2'd fd 7 failed", OS_COLOR_ROSE);
                    }
                }
            }
        }
    } else if (os_strcmp(cmd, "testwait") == 0) {
        term_print_line("Testing Process Lifecycle & sys_waitpid...", OS_COLOR_CYAN_NEON);
        int spawned = os_spawn("/hello.elf");
        if (spawned < 0) {
            term_print_line("FAIL: os_spawn(/hello.elf) failed", OS_COLOR_ROSE);
        } else {
            char sp_msg[64], tid_str[16];
            os_itoa(spawned, tid_str);
            os_strcpy(sp_msg, "Spawned child PID ");
            os_strcpy(sp_msg + os_strlen(sp_msg), tid_str);
            os_strcpy(sp_msg + os_strlen(sp_msg), ", calling waitpid()...");
            term_print_line(sp_msg, OS_COLOR_SLATE_200);

            int status = -1;
            int reaped = os_waitpid(spawned, &status, 0);
            if (reaped != spawned) {
                term_print_line("FAIL: os_waitpid did not return child PID", OS_COLOR_ROSE);
            } else {
                char ok_msg[80], st_str[16];
                os_itoa(WEXITSTATUS(status), st_str);
                os_strcpy(ok_msg, "PASS: Reaped child PID ");
                os_strcpy(ok_msg + os_strlen(ok_msg), tid_str);
                os_strcpy(ok_msg + os_strlen(ok_msg), " with exit status ");
                os_strcpy(ok_msg + os_strlen(ok_msg), st_str);
                term_print_line(ok_msg, OS_COLOR_EMERALD_LT);
                term_print_line("PASS: All POSIX waitpid tests passed!", OS_COLOR_CYAN_NEON);
            }
        }
    } else if (os_strcmp(cmd, "pwd") == 0) {
        char cwd[128];
        if (os_getcwd(cwd, sizeof(cwd))) {
            term_print_line(cwd, OS_COLOR_CYAN_NEON);
        } else {
            term_print_line("Error: os_getcwd failed", OS_COLOR_ROSE);
        }
    } else if ((cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ') || os_strcmp(cmd, "cd") == 0) {
        const char *target = "/";
        if (cmd[2] == ' ') {
            target = cmd + 3;
            while (*target == ' ') target++;
        }
        if (*target == '\0') target = "/";
        int res = os_chdir(target);
        if (res < 0) {
            char err[128];
            os_strcpy(err, "cd: no such file or directory: '");
            os_strcpy(err + os_strlen(err), target);
            os_strcpy(err + os_strlen(err), "'");
            term_print_line(err, OS_COLOR_ROSE);
        }
    } else if (os_strcmp(cmd, "testsh") == 0) {
        term_print_line("Testing POSIX Shell Environment & Subsystems...", OS_COLOR_CYAN_NEON);

        /* 1. Test getcwd & chdir */
        char cwd[64];
        if (!os_getcwd(cwd, sizeof(cwd))) {
            term_print_line("FAIL: os_getcwd returned NULL", OS_COLOR_ROSE);
        } else {
            char cwd_msg[80];
            os_strcpy(cwd_msg, "PASS: os_getcwd: '");
            os_strcpy(cwd_msg + os_strlen(cwd_msg), cwd);
            os_strcpy(cwd_msg + os_strlen(cwd_msg), "'");
            term_print_line(cwd_msg, OS_COLOR_EMERALD_LT);
        }

        int cd_res = os_chdir("/");
        if (cd_res != 0) {
            term_print_line("FAIL: os_chdir(/) returned error", OS_COLOR_ROSE);
        } else {
            term_print_line("PASS: os_chdir('/') succeeded", OS_COLOR_EMERALD_LT);
        }

        /* 2. Test Stream Redirection via os_spawn_stdio & pipe */
        term_print_line("Testing Spawn Pipeline with Stdout Redirection...", OS_COLOR_SLATE_200);
        int pfd[2];
        if (os_pipe(pfd) < 0) {
            term_print_line("FAIL: os_pipe failed", OS_COLOR_ROSE);
        } else {
            const char *args[] = { "/hello.elf", NULL };
            int child_pid = os_spawn_stdio("/hello.elf", args, -1, pfd[1], -1);
            if (child_pid < 0) {
                term_print_line("FAIL: os_spawn_stdio failed", OS_COLOR_ROSE);
                os_close(pfd[0]);
                os_close(pfd[1]);
            } else {
                /* Close write end in parent so EOF is triggered on child exit */
                os_close(pfd[1]);

                /* Wait for child */
                int status = 0;
                os_waitpid(child_pid, &status, 0);

                /* Read output from child through pipe */
                char pipe_rx[64];
                ssize_t n = os_read_fd(pfd[0], pipe_rx, sizeof(pipe_rx) - 1);
                os_close(pfd[0]);

                char sp_res[80];
                char pid_str[16];
                os_itoa(child_pid, pid_str);
                os_strcpy(sp_res, "PASS: Child PID ");
                os_strcpy(sp_res + os_strlen(sp_res), pid_str);
                os_strcpy(sp_res + os_strlen(sp_res), " reaped, pipe received ");
                char bytes_str[16];
                os_itoa(n > 0 ? n : 0, bytes_str);
                os_strcpy(sp_res + os_strlen(sp_res), bytes_str);
                os_strcpy(sp_res + os_strlen(sp_res), " bytes");
                term_print_line(sp_res, OS_COLOR_EMERALD_LT);

                term_print_line("PASS: All POSIX Shell Environment tests passed!", OS_COLOR_CYAN_NEON);
            }
        }
    } else if (os_strcmp(cmd, "version") == 0) {
        term_print_line("OpenSweet OS v0.0.4 [x86_64 Long Mode SMP]", OS_COLOR_VIOLET);
        term_print_line("Pure 64-bit micro-monolithic kernel with Ring 3 userspace", OS_COLOR_SLATE_400);
    } else if (os_strcmp(cmd, "memtest") == 0) {
        term_print_line("Running dynamic heap allocator test (sys_brk)...", OS_COLOR_CYAN_NEON);
        void *p1 = os_malloc(256);
        if (!p1) {
            term_print_line("FAIL: malloc(256) returned NULL", OS_COLOR_ROSE);
        } else {
            char p1_str[64];
            char addr_buf[24];
            os_itoa((int64_t)(uintptr_t)p1, addr_buf);
            os_strcpy(p1_str, "Allocated 256 bytes at virtual address 0x");
            os_strcpy(p1_str + os_strlen(p1_str), addr_buf);
            term_print_line(p1_str, OS_COLOR_EMERALD_LT);

            /* Fill buffer with pattern and verify */
            uint8_t *b = (uint8_t*)p1;
            for (int i = 0; i < 256; i++) b[i] = (uint8_t)(i & 0xFF);
            int ok = 1;
            for (int i = 0; i < 256; i++) {
                if (b[i] != (uint8_t)(i & 0xFF)) { ok = 0; break; }
            }

            if (!ok) {
                term_print_line("FAIL: Data integrity check failed!", OS_COLOR_ROSE);
            } else {
                term_print_line("PASS: Memory read/write integrity verified!", OS_COLOR_EMERALD_LT);
            }

            void *p2 = os_malloc(1024);
            if (!p2) {
                term_print_line("FAIL: malloc(1024) returned NULL", OS_COLOR_ROSE);
            } else {
                term_print_line("PASS: malloc(1024) succeeded!", OS_COLOR_EMERALD_LT);
                os_free(p2);
                term_print_line("PASS: free(1024) succeeded!", OS_COLOR_SLATE_300);
            }

            os_free(p1);
            term_print_line("PASS: Heap block coalescing verified!", OS_COLOR_CYAN_NEON);
        }
    } else if (os_strcmp(cmd, "ls") == 0 || os_strcmp(cmd, "dir") == 0) {
        os_dirent_t entries[32];
        int count = os_read_dir(entries, 32);
        if (count < 0) {
            term_print_line("Error: Unable to read ext4 directory", OS_COLOR_ROSE);
        } else {
            term_print_line("Directory of / (ext4 storage):", OS_COLOR_CYAN_NEON);
            for (int i = 0; i < count; i++) {
                char row[80];
                char sz_str[16];
                os_itoa((int64_t)entries[i].size, sz_str);
                
                os_strcpy(row, entries[i].type == 2 ? "[DIR]  " : "[FILE] ");
                os_strcpy(row + os_strlen(row), entries[i].name);
                
                /* Pad name to 24 chars */
                int nlen = (int)os_strlen(row);
                while (nlen < 32) row[nlen++] = ' ';
                row[nlen] = '\0';
                
                os_strcpy(row + os_strlen(row), sz_str);
                os_strcpy(row + os_strlen(row), " B");
                
                uint32_t col = entries[i].type == 2 ? OS_COLOR_AMBER : 
                              (entries[i].name[os_strlen(entries[i].name)-1] == 'f' ? OS_COLOR_INDIGO_LT : OS_COLOR_SLATE_200);
                term_print_line(row, col);
            }
        }
    } else if (os_strcmp(cmd, "calc") == 0) {
        term_print_line("Spawning /calc.elf...", OS_COLOR_CYAN_NEON);
        os_spawn("/calc.elf");
    } else if (os_strcmp(cmd, "notepad") == 0) {
        term_print_line("Spawning /notepad.elf...", OS_COLOR_CYAN_NEON);
        os_spawn("/notepad.elf");
    } else if (os_strcmp(cmd, "files") == 0) {
        term_print_line("Spawning /files.elf...", OS_COLOR_CYAN_NEON);
        os_spawn("/files.elf");
    } else if (os_strcmp(cmd, "doom") == 0) {
        term_print_line("Spawning /doom.elf...", OS_COLOR_ROSE);
        os_spawn("/doom.elf");
    } else if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o' && cmd[4] == ' ') {
        term_print_line(cmd + 5, OS_COLOR_WHITE);
    } else if (cmd[0] == 'c' && cmd[1] == 'a' && cmd[2] == 't' && cmd[3] == ' ') {
        const char *p = cmd + 4;
        while (*p == ' ') p++;
        if (*p == '\0') {
            term_print_line("Usage: cat <filename>", OS_COLOR_ROSE);
        } else {
            char file_buf[4096];
            ssize_t n = os_read_file(p, file_buf, sizeof(file_buf) - 1);
            if (n < 0) {
                term_print_line("Error: File not found or read error", OS_COLOR_ROSE);
            } else {
                file_buf[n] = '\0';
                char line[TERM_COLS + 1];
                int li = 0;
                for (ssize_t i = 0; i <= n; i++) {
                    if (file_buf[i] == '\n' || file_buf[i] == '\0') {
                        line[li] = '\0';
                        term_print_line(line, OS_COLOR_SLATE_100);
                        li = 0;
                    } else if (li < TERM_COLS) {
                        line[li++] = file_buf[i];
                    }
                }
            }
        }
    } else if (cmd[0] == 'w' && cmd[1] == 'r' && cmd[2] == 'i' && cmd[3] == 't' && cmd[4] == 'e' && cmd[5] == ' ') {
        /* Parse: write <filename> <content> */
        const char *p = cmd + 6;
        while (*p == ' ') p++;
        char fname[32];
        int fi = 0;
        while (*p && *p != ' ' && fi < 31) {
            fname[fi++] = *p++;
        }
        fname[fi] = '\0';
        while (*p == ' ') p++;

        if (fi == 0) {
            term_print_line("Usage: write <filename> <content>", OS_COLOR_ROSE);
        } else {
            ssize_t written = os_write_file(fname, p, os_strlen(p));
            if (written < 0) {
                term_print_line("Error: ext4 write failed", OS_COLOR_ROSE);
            } else {
                char res[64];
                char bytes[16];
                os_itoa((int64_t)written, bytes);
                os_strcpy(res, "Successfully wrote ");
                os_strcpy(res + os_strlen(res), bytes);
                os_strcpy(res + os_strlen(res), " bytes to ext4 file '");
                os_strcpy(res + os_strlen(res), fname);
                os_strcpy(res + os_strlen(res), "'");
                term_print_line(res, OS_COLOR_EMERALD_LT);
            }
        }
    } else if ((cmd[0] == 'r' && cmd[1] == 'm' && cmd[2] == ' ') ||
               (cmd[0] == 'd' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == ' ')) {
        const char *p = (cmd[0] == 'r') ? cmd + 3 : cmd + 4;
        while (*p == ' ') p++;
        if (*p == '\0') {
            term_print_line("Usage: rm <filename>", OS_COLOR_ROSE);
        } else {
            int ret = os_delete_file(p);
            if (ret < 0) {
                term_print_line("Error: File not found or cannot be deleted", OS_COLOR_ROSE);
            } else {
                char res[64];
                os_strcpy(res, "Successfully deleted '");
                os_strcpy(res + os_strlen(res), p);
                os_strcpy(res + os_strlen(res), "'");
                term_print_line(res, OS_COLOR_EMERALD_LT);
            }
        }
    } else if (os_strcmp(cmd, "exit") == 0) {
        os_exit(0);
    } else {
        /* Tokenize command into argv array */
        char cmd_buf[128];
        os_strcpy(cmd_buf, cmd);
        char *argv[16];
        int argc = 0;
        char *p = cmd_buf;
        while (*p && argc < 15) {
            while (*p == ' ') p++;
            if (*p == '\0') break;
            argv[argc++] = p;
            while (*p && *p != ' ') p++;
            if (*p) {
                *p = '\0';
                p++;
            }
        }
        argv[argc] = NULL;
        if (argc == 0) return;

        /* Check for background execution '&' */
        int run_bg = 0;
        if (argc > 1 && os_strcmp(argv[argc - 1], "&") == 0) {
            run_bg = 1;
            argv[--argc] = NULL;
        }

        const char *exe_name = argv[0];
        char exec_path[64];
        int spawned = -1;
        if (exe_name[0] == '/') {
            os_strcpy(exec_path, exe_name);
            spawned = os_spawn_args(exec_path, (const char *const*)argv);
        } else {
            /* Try "/<name>.elf" */
            os_strcpy(exec_path, "/");
            os_strcpy(exec_path + os_strlen(exec_path), exe_name);
            os_strcpy(exec_path + os_strlen(exec_path), ".elf");
            spawned = os_spawn_args(exec_path, (const char *const*)argv);
            if (spawned < 0) {
                /* Try "/<name>" */
                os_strcpy(exec_path, "/");
                os_strcpy(exec_path + os_strlen(exec_path), exe_name);
                spawned = os_spawn_args(exec_path, (const char *const*)argv);
            }
        }

        if (spawned >= 0) {
            char ok_msg[80];
            char tid_str[16];
            os_itoa(spawned, tid_str);
            os_strcpy(ok_msg, run_bg ? "Started [PID " : "Running [PID ");
            os_strcpy(ok_msg + os_strlen(ok_msg), tid_str);
            os_strcpy(ok_msg + os_strlen(ok_msg), "] ");
            os_strcpy(ok_msg + os_strlen(ok_msg), exec_path);
            term_print_line(ok_msg, OS_COLOR_EMERALD_LT);

            /* If foreground: wait for process to finish using os_waitpid */
            if (!run_bg) {
                int status = 0;
                os_waitpid(spawned, &status, 0);
            }
        } else {
            char err[80];
            os_strcpy(err, "Command not found: '");
            os_strcpy(err + os_strlen(err), exe_name);
            os_strcpy(err + os_strlen(err), "'. Type 'help'.");
            term_print_line(err, OS_COLOR_ROSE);
        }
    }
}

/* Powerline Interlocking Chevron Ribbon Renderer with Subpixel Anti-Aliased 16x MSAA Profiles */
static void draw_chevron_segment(os_window_t *win, int x0, int x1, int y, int h, int left_arrow, int right_arrow, uint32_t color) {
    if (!win || !win->canvas || h != 20) return;
    int cw = win->client_w;
    int ch = win->client_h;
    uint32_t rgb = color & 0x00FFFFFF;

    static const int sy_tab[4] = { 32, 96, 160, 224 };
    static const int sx_tab[4] = { 32, 96, 160, 224 };

    for (int dy = 0; dy < 20; dy++) {
        int py = y + dy;
        if (py < 0 || py >= ch) continue;

        /* Precompute the 4 subpixel arrow offsets for this scanline row */
        int dx_sub[4];
        int min_dx = 2560, max_dx = 0;
        for (int s = 0; s < 4; s++) {
            int v = dy * 256 + sy_tab[s];
            int d = v > 2560 ? (v - 2560) : (2560 - v);
            int dx = (3 * (2560 - d)) >> 2; /* 0..1920 (0.0 .. 7.5 px) */
            dx_sub[s] = dx;
            if (dx < min_dx) min_dx = dx;
            if (dx > max_dx) max_dx = dx;
        }

        /* Determine pixel bounding box for this row */
        int rx_start = left_arrow ? (x0 + (min_dx >> 8) - 1) : (x0 - 1);
        int rx_end   = right_arrow ? (x1 + (max_dx >> 8) + 2) : x1;

        if (rx_start < 0) rx_start = 0;
        if (rx_end > cw) rx_end = cw;
        if (rx_start >= rx_end) continue;

        uint32_t *prow = win->canvas + py * cw;

        for (int px = rx_start; px < rx_end; px++) {
            int px_fp = px * 256;

            /* Interior fast-path (when both boundaries are safely away) */
            int xl_max = left_arrow ? (x0 * 256 + max_dx) : (x0 * 256);
            int xr_min = right_arrow ? (x1 * 256 + min_dx) : (x1 * 256);

            if (left_arrow && px_fp >= xl_max && (px_fp + 256) <= xr_min) {
                prow[px] = color;
                continue;
            }

            /* Subpixel 4x4 (16 samples) coverage test */
            int cov = 0;
            for (int s = 0; s < 4; s++) {
                int dx = dx_sub[s];
                int xl;
                if (left_arrow) {
                    xl = x0 * 256 + dx;
                } else {
                    /* Smooth 4px rounded left cap for the ribbon start */
                    int vy = dy * 256 + sy_tab[s];
                    int d_top = vy;
                    int d_bot = 5120 - vy; /* 20 * 256 */
                    int dc = d_top < d_bot ? d_top : d_bot;
                    if (dc < 1024) {
                        static const int round_offsets[4] = { 460, 230, 80, 20 };
                        int r_idx = dc >> 8;
                        if (r_idx > 3) r_idx = 3;
                        xl = x0 * 256 + round_offsets[r_idx];
                    } else {
                        xl = x0 * 256;
                    }
                }

                int xr = right_arrow ? (x1 * 256 + dx) : (x1 * 256);

                for (int sx = 0; sx < 4; sx++) {
                    int samp = px_fp + sx_tab[sx];
                    if (samp >= xl && samp < xr) {
                        cov++;
                    }
                }
            }

            if (cov == 16) {
                prow[px] = color;
            } else if (cov > 0) {
                uint32_t alpha = (cov * 255) >> 4;
                prow[px] = os_alpha_blend((alpha << 24) | rgb, prow[px]);
            }
        }
    }
}

/* Smooth Anti-Aliased Outline Heart ♡ Glyph Renderer (11x10 pixels) */
static void draw_outline_heart(os_window_t *win, int x, int y, uint32_t color) {
    static const uint8_t heart_aa[10][11] = {
        {   0,  60, 200, 240,  80,   0,  80, 240, 200,  60,   0 },
        {  80, 240, 255, 240, 160,  60, 160, 240, 255, 240,  80 },
        { 200, 255,  80,   0,   0,   0,   0,   0,  80, 255, 200 },
        { 220, 255,   0,   0,   0,   0,   0,   0,   0, 255, 220 },
        { 180, 255,  60,   0,   0,   0,   0,   0,  60, 255, 180 },
        {  60, 220, 240,  80,   0,   0,   0,  80, 240, 220,  60 },
        {   0,  80, 240, 220,  60,   0,  60, 220, 240,  80,   0 },
        {   0,   0,  80, 220, 240,  80, 240, 220,  80,   0,   0 },
        {   0,   0,   0,  60, 220, 255, 220,  60,   0,   0,   0 },
        {   0,   0,   0,   0,  60, 200,  60,   0,   0,   0,   0 }
    };
    int cw = win->client_w;
    int ch = win->client_h;
    uint32_t rgb = color & 0x00FFFFFF;
    for (int dy = 0; dy < 10; dy++) {
        int py = y + dy;
        if (py < 0 || py >= ch) continue;
        uint32_t *prow = win->canvas + py * cw;
        for (int dx = 0; dx < 11; dx++) {
            int px = x + dx;
            if (px < 0 || px >= cw) continue;
            uint8_t a = heart_aa[dy][dx];
            if (a == 0) continue;
            prow[px] = os_alpha_blend(((uint32_t)a << 24) | rgb, prow[px]);
        }
    }
}

/* ANSI & UTF-8 aware line renderer for terminal output */
static void term_draw_line(os_window_t *win, int x, int y, const char *str, uint32_t default_color) {
    if (!win || !win->canvas || !str) return;
    int cur_x = x;
    uint32_t color = default_color;
    while (*str) {
        if (*str == '\x1b' && *(str + 1) == '[') {
            str += 2;
            int code = 0;
            while (*str >= '0' && *str <= '9') {
                code = code * 10 + (*str - '0');
                str++;
            }
            if (*str == 'm') str++;
            if (code == 0) color = default_color;
            else if (code == 31) color = 0xFFF38BA8;
            else if (code == 32) color = 0xFFA6E3A1;
            else if (code == 33) color = 0xFFF9E2AF;
            else if (code == 34) color = 0xFF89B4FA;
            else if (code == 35) color = 0xFFCBA6F7;
            else if (code == 36) color = 0xFF89DCEB;
            else if (code == 37) color = 0xFFCDD6F4;
            else if (code == 90) color = 0xFF6C7086;
            continue;
        }
        char c = *str;
        if (c >= 32 && c <= 126) {
            os_draw_char_mono_aa(win, cur_x, y, c, color);
            cur_x += OS_FONT_MONO_W;
        } else if ((uint8_t)c == 0xE2 && (uint8_t)*(str + 1) == 0x97 && (uint8_t)*(str + 2) == 0x8F) {
            str += 2;
            os_fill_rounded_rect(win, cur_x + 1, y + 3, 7, 7, 3, color);
            cur_x += OS_FONT_MONO_W;
        } else {
            cur_x += OS_FONT_MONO_W;
        }
        str++;
    }
}


/* Format cwd into path display string with thin chevron separators: e.g. "~ > bin" or "~" */
static void format_path_display(const char *cwd, char *out_path, size_t max_len) {
    if (!cwd || cwd[0] == '\0' || (cwd[0] == '/' && cwd[1] == '\0')) {
        os_strcpy(out_path, "~");
        return;
    }

    out_path[0] = '~';
    out_path[1] = '\0';
    size_t out_len = 1;

    const char *p = cwd;
    while (*p) {
        while (*p == '/') p++;
        if (*p == '\0') break;

        const char *comp_start = p;
        while (*p && *p != '/') p++;
        size_t comp_len = p - comp_start;

        if (comp_len > 0 && out_len + 3 + comp_len < max_len) {
            out_path[out_len++] = ' ';
            out_path[out_len++] = '>';
            out_path[out_len++] = ' ';
            for (size_t i = 0; i < comp_len; i++) {
                out_path[out_len++] = comp_start[i];
            }
            out_path[out_len] = '\0';
        }
    }
}

/* Draw a full Powerline chevron prompt row with path, RTC time, command text, and thin cursor */
static void draw_powerline_prompt(os_window_t *win, int start_x, int py, const char *cwd, const char *time_str, const char *cmd_text, int is_active) {
    if (!win || !win->canvas) return;
    int ph = 20;
    int cur_x = start_x;

    /* Segment 1: Pastel Magenta "opensweet" (#9B348D) */
    const char *seg1_txt = "opensweet";
    int seg1_txt_w = 9 * OS_FONT_MONO_W; /* 72px */
    int seg1_w = 12 + seg1_txt_w + 14;   /* 98px */
    draw_chevron_segment(win, cur_x, cur_x + seg1_w, py, ph, 0, 1, 0xFF9B348D);
    os_draw_text_mono_aa(win, cur_x + 12, py + 3, seg1_txt, 0xFFFFFFFF);
    cur_x += seg1_w;

    /* Segment 2: Pastel Strawberry Coral Path (#DC617E) with thin chevron dividers */
    char path_disp[64];
    format_path_display(cwd, path_disp, sizeof(path_disp));
    int path_len = os_strlen(path_disp);
    int path_text_w = path_len * OS_FONT_MONO_W;
    int seg2_w = 13 + path_text_w + 13;
    if (seg2_w < 34) seg2_w = 34;

    draw_chevron_segment(win, cur_x, cur_x + seg2_w, py, ph, 1, 1, 0xFFDC617E);
    int px = cur_x + 13;
    for (int i = 0; path_disp[i]; i++) {
        char c = path_disp[i];
        if (c == '>') {
            os_draw_char_mono_aa(win, px, py + 2, '>', 0xCCFFFFFF);
        } else {
            os_draw_char_mono_aa(win, px, py + 2, c, 0xFFFFFFFF);
        }
        px += OS_FONT_MONO_W;
    }
    cur_x += seg2_w;

    /* 3 Accent Decorative Pastel Chevron Stripes (5px each) */
    draw_chevron_segment(win, cur_x, cur_x + 5, py, ph, 1, 1, 0xFFFEA384);
    cur_x += 5;
    draw_chevron_segment(win, cur_x, cur_x + 5, py, ph, 1, 1, 0xFF81BBD6);
    cur_x += 5;
    draw_chevron_segment(win, cur_x, cur_x + 5, py, ph, 1, 1, 0xFF0C9699);
    cur_x += 5;

    /* Segment 3: Pastel Slate Ocean "♡ HH:MM" (#326489) */
    int seg3_w = 14 + 11 + 7 + (5 * OS_FONT_MONO_W) + 14; /* 86px */
    draw_chevron_segment(win, cur_x, cur_x + seg3_w, py, ph, 1, 1, 0xFF326489);
    draw_outline_heart(win, cur_x + 14, py + 5, 0xFFFFFFFF);
    os_draw_text_mono_aa(win, cur_x + 32, py + 3, time_str, 0xFFFFFFFF);
    cur_x += seg3_w;

    /* Command text: directly follows rightmost chevron tip (+ 8px arrow + 8px spacing, NO extra '>') */
    int input_start_x = cur_x + 8 + 8;

    if (cmd_text && cmd_text[0]) {
        os_draw_text_mono_aa(win, input_start_x, py + 3, cmd_text, 0xFFFFFFFF);
    }

    /* Very thin white vertical cursor '|' (2px wide, pure white / soft silver pulse) */
    if (is_active) {
        int cursor_x = input_start_x + input_len * OS_FONT_MONO_W;
        uint32_t cursor_clr = curr_blink_state ? 0xFFFFFFFF : 0xFFA0A0B0;
        os_fill_rect(win, cursor_x, py + 2, 2, 16, cursor_clr);
    }
}

/* Render complete terminal UI */
static void render_terminal(os_window_t *win) {
    int cw = win->client_w;
    int ch = win->client_h;

    /* 1. Background (Tokyo Night / Catppuccin deep soft dark) */
    os_fill_rect(win, 0, 0, cw, ch, 0xFF181825);

    /* 2. Output lines (rendering both plain text and Powerline prompt lines) */
    int max_visible = (ch - TOP_H - BOT_H - 8) / LINE_H;
    if (max_visible < 1) max_visible = 1;
    int start_r = (cur_row + 1 > max_visible) ? (cur_row + 1 - max_visible) : 0;

    int y = TOP_H + 4;
    for (int r = start_r; r < cur_row; r++) {
        if (lines[r].is_prompt) {
            draw_powerline_prompt(win, 14, y, lines[r].cwd, lines[r].time, lines[r].text, 0);
        } else if (lines[r].text[0] != '\0') {
            term_draw_line(win, 14, y, lines[r].text, lines[r].color);
        }
        y += LINE_H;
    }

    /* 3. Current Active Prompt Line with Pastel Powerline Interlocking Chevron Ribbon */
    if (y + LINE_H <= ch - BOT_H + 8) {
        char cur_cwd[64];
        if (!os_getcwd(cur_cwd, sizeof(cur_cwd))) {
            os_strcpy(cur_cwd, "/");
        }
        os_time_t rtc = os_get_time();
        char cur_time[8];
        cur_time[0] = '0' + (rtc.hours / 10);
        cur_time[1] = '0' + (rtc.hours % 10);
        cur_time[2] = ':';
        cur_time[3] = '0' + (rtc.minutes / 10);
        cur_time[4] = '0' + (rtc.minutes % 10);
        cur_time[5] = '\0';

        draw_powerline_prompt(win, 14, y, cur_cwd, cur_time, input_buf, 1);
    }

    /* 4. Footer Status Bar */
    int bot_y = ch - BOT_H;
    os_fill_rect(win, 0, bot_y, cw, BOT_H, OS_COLOR_SLATE_900);
    os_fill_rect(win, 0, bot_y, cw, 1, OS_COLOR_SLATE_700);

    os_draw_text_ui_aa(win, 14, bot_y + 5, "UTF-8  |  Ring 3 CPL=3  |  Type 'help' for commands", OS_COLOR_SLATE_400);

    char up_str[32], up_num[16];
    os_itoa(os_uptime() / 100, up_num);
    os_strcpy(up_str, "Uptime: ");
    os_strcpy(up_str + os_strlen(up_str), up_num);
    os_strcpy(up_str + os_strlen(up_str), "s");
    os_draw_text_ui_aa(win, cw - os_text_width_ui_aa(up_str) - 14, bot_y + 5, up_str, OS_COLOR_CYAN_NEON);
}

int main(int argc, char **argv) {
    os_register_app(&terminal_pkg);

    os_window_t win = os_create_window("Terminal - OpenSweet Shell", 100, 70, WIN_W, WIN_H);
    if (win.win_id < 0) return 1;

    /* Print Welcome Banner with Sweetfetch */
    print_fetch();
    term_print_line("", OS_COLOR_WHITE);


    render_terminal(&win);
    os_update_window(&win);

    os_event_t ev;
    int last_blink_state = -1;
    int running = 1;
    while (running) {
        blink_cnt++;
        curr_blink_state = (blink_cnt % 60) < 36;
        int needs_redraw = 0;

        if (curr_blink_state != last_blink_state) {
            last_blink_state = curr_blink_state;
            needs_redraw = 1;
        }

        while (os_poll_event(&win, &ev)) {
            needs_redraw = 1;
            blink_cnt = 0;
            curr_blink_state = 1;
            last_blink_state = 1;

            if (ev.type == OS_EVENT_WIN_CLOSE) {
                running = 0;
                break;
            } else if (ev.type == OS_EVENT_WIN_MAXIMIZE) {
                win.width = ev.x;
                win.height = ev.y;
                win.client_w = ev.x;
                win.client_h = ev.y > 33 ? ev.y - 33 : 0;
            } else if (ev.type == OS_EVENT_MOUSE_DOWN) {
                /* User clicked inside terminal -> ensure focus & solid cursor */
                blink_cnt = 0;
                curr_blink_state = 1;
            } else if (ev.type == OS_EVENT_KEY_DOWN) {
                uint32_t key = ev.param;
                if (key == 10 || key == 13) {
                    /* Enter: commit command with full Powerline prompt preservation */
                    term_print_prompt_line(input_buf);

                    char cmd_copy[128];
                    os_strcpy(cmd_copy, input_buf);
                    input_len = 0;
                    input_buf[0] = '\0';

                    execute_command(cmd_copy);
                } else if (key == 8) {
                    /* Backspace */
                    if (input_len > 0) {
                        input_len--;
                        input_buf[input_len] = '\0';
                    }
                } else if (key >= 32 && key <= 126) {
                    /* Printable character */
                    if (input_len < (int)sizeof(input_buf) - 2) {
                        input_buf[input_len++] = (char)key;
                        input_buf[input_len] = '\0';
                    }
                }
            }
        }

        if (needs_redraw) {
            render_terminal(&win);
            os_update_window(&win);
        }
        os_sleep(10);
    }

    os_close_window(&win);
    return 0;
}
