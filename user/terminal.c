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

#define WIN_W 700
#define WIN_H 480

#define TERM_ROWS 20
#define TERM_COLS 82
#define LINE_H    20
#define TOP_H     34
#define BOT_H     26

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
} term_line_t;

static term_line_t lines[TERM_ROWS];
static int         cur_row = 0;
static char        input_buf[128];
static int         input_len = 0;
static int         blink_cnt = 0;

static void term_scroll_up(void) {
    for (int r = 0; r < TERM_ROWS - 1; r++) {
        lines[r] = lines[r + 1];
    }
    lines[TERM_ROWS - 1].text[0] = '\0';
    lines[TERM_ROWS - 1].color = OS_COLOR_SLATE_200;
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
    cur_row++;
}

static void term_clear(void) {
    for (int r = 0; r < TERM_ROWS; r++) {
        lines[r].text[0] = '\0';
        lines[r].color = OS_COLOR_SLATE_200;
    }
    cur_row = 0;
    input_len = 0;
    input_buf[0] = '\0';
}

/* Execute shell command */
static void execute_command(const char *cmd) {
    /* Skip leading whitespace */
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    if (os_strcmp(cmd, "help") == 0) {
        term_print_line("OpenSweet OS Shell Commands:", OS_COLOR_CYAN_NEON);
        term_print_line("  help          Show this list of available commands", OS_COLOR_SLATE_300);
        term_print_line("  ls / dir      List files and folders on ext4 filesystem", OS_COLOR_SLATE_300);
        term_print_line("  clear / cls   Clear the terminal screen", OS_COLOR_SLATE_300);
        term_print_line("  echo <text>   Print text to console", OS_COLOR_SLATE_300);
        term_print_line("  uptime        Display system uptime in seconds", OS_COLOR_SLATE_300);
        term_print_line("  calc          Launch Desktop Calculator (Ring 3)", OS_COLOR_SLATE_300);
        term_print_line("  notepad       Launch Desktop Notepad (Ring 3)", OS_COLOR_SLATE_300);
        term_print_line("  files         Launch File Explorer (Ring 3)", OS_COLOR_SLATE_300);
        term_print_line("  version       Show kernel architecture and build", OS_COLOR_SLATE_300);
        term_print_line("  exit          Close terminal window", OS_COLOR_SLATE_300);
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
        term_print_line(buf, OS_COLOR_EMERALD_LT);
    } else if (os_strcmp(cmd, "version") == 0) {
        term_print_line("OpenSweet OS v0.0.4 [x86_64 Long Mode SMP]", OS_COLOR_VIOLET);
        term_print_line("Pure 64-bit micro-monolithic kernel with Ring 3 userspace", OS_COLOR_SLATE_400);
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
    } else if (cmd[0] == 'e' && cmd[1] == 'c' && cmd[2] == 'h' && cmd[3] == 'o' && cmd[4] == ' ') {
        term_print_line(cmd + 5, OS_COLOR_WHITE);
    } else if (os_strcmp(cmd, "exit") == 0) {
        os_exit(0);
    } else {
        char err[80];
        os_strcpy(err, "Command not found: '");
        os_strcpy(err + os_strlen(err), cmd);
        os_strcpy(err + os_strlen(err), "'. Type 'help'.");
        term_print_line(err, OS_COLOR_ROSE);
    }
}

/* Render complete terminal UI */
static void render_terminal(os_window_t *win) {
    int cw = win->client_w;
    int ch = win->client_h;

    /* 1. Background */
    os_fill_rect(win, 0, 0, cw, ch, OS_COLOR_OBSIDIAN);

    /* 2. Header Bar */
    os_fill_gradient_v(win, 0, 0, cw, TOP_H, OS_COLOR_SLATE_900, OS_COLOR_SLATE_950);
    os_fill_rect(win, 0, TOP_H - 1, cw, 1, OS_COLOR_SLATE_700);

    /* Session Tab Chip with Segoe UI text */
    os_draw_badge(win, 12, 7, "SHELL", OS_COLOR_INDIGO, OS_COLOR_WHITE);
    os_draw_text_ui_aa(win, 84, 9, "opensweet@localhost: ~ [x86_64 SMP]", OS_COLOR_SLATE_300);

    /* 3. Output lines with Consolas 15px Anti-Aliased Typography */
    int y = TOP_H + 8;
    for (int r = 0; r < cur_row; r++) {
        if (lines[r].text[0] != '\0') {
            os_draw_text_mono_aa(win, 16, y, lines[r].text, lines[r].color);
        }
        y += LINE_H;
        if (y + LINE_H > ch - BOT_H) break;
    }

    /* 4. Current Prompt Line */
    if (y + LINE_H <= ch - BOT_H) {
        const char *prompt = "opensweet:~$ ";
        os_draw_text_mono_aa(win, 16, y, prompt, OS_COLOR_EMERALD_LT);
        int prompt_w = (int)os_strlen(prompt) * OS_FONT_MONO_W;
        os_draw_text_mono_aa(win, 16 + prompt_w, y, input_buf, OS_COLOR_WHITE);

        /* Smooth Blinking Block Cursor */
        if ((blink_cnt % 30) < 18) {
            int cx = 16 + prompt_w + input_len * OS_FONT_MONO_W;
            os_fill_rounded_rect(win, cx, y + 1, 8, 14, 2, OS_COLOR_CYAN_NEON);
        }
    }

    /* 5. Footer Status Bar */
    int bot_y = ch - BOT_H;
    os_fill_rect(win, 0, bot_y, cw, BOT_H, OS_COLOR_SLATE_900);
    os_fill_rect(win, 0, bot_y, cw, 1, OS_COLOR_SLATE_700);

    os_draw_text_ui_aa(win, 16, bot_y + 5, "UTF-8  |  Ring 3 CPL=3  |  Type 'help' for commands", OS_COLOR_SLATE_400);

    char up_str[32], up_num[16];
    os_itoa(os_uptime() / 100, up_num);
    os_strcpy(up_str, "Uptime: ");
    os_strcpy(up_str + os_strlen(up_str), up_num);
    os_strcpy(up_str + os_strlen(up_str), "s");
    os_draw_text_ui_aa(win, cw - os_text_width_ui_aa(up_str) - 16, bot_y + 5, up_str, OS_COLOR_CYAN_NEON);
}

int main(void) {
    os_register_app(&terminal_pkg);

    os_window_t win = os_create_window("Terminal - OpenSweet Shell", 100, 70, WIN_W, WIN_H);
    if (win.win_id < 0) return 1;

    /* Print Welcome Banner */
    term_print_line("OpenSweet OS v0.0.4 [Interactive C Shell]", OS_COLOR_CYAN_NEON);
    term_print_line("Copyright (c) 2026 OpenSweet Team. All rights reserved.", OS_COLOR_SLATE_400);
    term_print_line("Type 'help' to display a list of built-in commands.", OS_COLOR_EMERALD_LT);
    term_print_line("", OS_COLOR_WHITE);

    render_terminal(&win);
    os_update_window(&win);

    os_event_t ev;
    while (1) {
        int has_event = os_poll_event(&win, &ev);
        blink_cnt++;

        if (has_event) {
            if (ev.type == OS_EVENT_WIN_CLOSE) {
                break;
            } else if (ev.type == OS_EVENT_KEY_DOWN) {
                uint32_t key = ev.param;
                if (key == 10 || key == 13) {
                    /* Enter: commit command */
                    char full_cmd[128 + 16];
                    os_strcpy(full_cmd, "opensweet:~$ ");
                    os_strcpy(full_cmd + os_strlen(full_cmd), input_buf);
                    term_print_line(full_cmd, OS_COLOR_SLATE_400);

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

        render_terminal(&win);
        os_update_window(&win);
        os_sleep(16); /* ~60 FPS */
    }

    os_close_window(&win);
    return 0;
}
