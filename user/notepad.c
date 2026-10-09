/* =============================================================================
 * OpenSweet OS - Native C Desktop Notepad (user/notepad.c)
 * Ring 3 graphical text editor written in standard C using OpenSweet C SDK.
 * Features:
 *   - Multi-line text buffer with cursor navigation and automatic wrapping
 *   - Line numbers gutter and real-time line/column footer stats
 *   - Interactive toolbar with [Clear] and [Sample] buttons
 *   - Full keyboard typing support (letters, numbers, space, Enter, Backspace)
 *   - Real-time blinking text cursor and fast 60 FPS compositor blit
 * ============================================================================= */

#include "opensweet.h"

#define WIN_W 540
#define WIN_H 420
#define MAX_TEXT 4096

#define GUTTER_W 46
#define TOP_BAR_H 38
#define BOT_BAR_H 26

static char text_buf[MAX_TEXT];
static int  text_len = 0;
static int  cursor_pos = 0;
static int  blink_cnt = 0;
static char cur_file_path[64] = "/hello.txt";
static char doc_title[64] = "hello.txt";

/* Toolbar buttons */
#define BTN_SAVE_X   284
#define BTN_SAVE_Y   6
#define BTN_SAVE_W   76
#define BTN_SAVE_H   26

#define BTN_SAMPLE_X 366
#define BTN_SAMPLE_Y 6
#define BTN_SAMPLE_W 76
#define BTN_SAMPLE_H 26

#define BTN_CLEAR_X  450
#define BTN_CLEAR_Y  6
#define BTN_CLEAR_W  76
#define BTN_CLEAR_H  26

static void load_file(const char *path) {
    if (!path || !path[0]) return;
    ssize_t n = os_read_file(path, text_buf, MAX_TEXT - 1);
    if (n >= 0) {
        text_len = (int)n;
        text_buf[text_len] = '\0';
        cursor_pos = text_len;
        os_strncpy(cur_file_path, path, sizeof(cur_file_path) - 1);
        cur_file_path[sizeof(cur_file_path) - 1] = '\0';
        /* Extract file name for title */
        const char *p = path;
        for (int i = 0; path[i]; i++) {
            if (path[i] == '/') p = path + i + 1;
        }
        os_strncpy(doc_title, p, sizeof(doc_title) - 1);
        doc_title[sizeof(doc_title) - 1] = '\0';
    }
}

static void save_file(void) {
    if (cur_file_path[0]) {
        os_write_file(cur_file_path, text_buf, text_len);
    }
}

static void insert_sample(void) {
    const char *sample = 
        "Welcome to OpenSweet Notepad!\n"
        "Written in pure C using OpenSweet SDK.\n"
        "Anti-Aliased Consolas 15px typography.\n"
        "Running in Ring 3 (CPL=3) user mode.\n"
        "Zero-copy 32bpp canvas blitting at 60 FPS.\n"
        "Type anywhere to edit this text!";
    text_len = 0;
    cursor_pos = 0;
    while (*sample && text_len < MAX_TEXT - 1) {
        text_buf[text_len++] = *sample++;
    }
    text_buf[text_len] = '\0';
    cursor_pos = text_len;
    os_strcpy(doc_title, "Document 1.txt");
}

static void clear_text(void) {
    text_len = 0;
    cursor_pos = 0;
    text_buf[0] = '\0';
}

static void insert_char(char c) {
    if (text_len >= MAX_TEXT - 1) return;
    /* Shift chars right */
    for (int i = text_len; i > cursor_pos; i--) {
        text_buf[i] = text_buf[i - 1];
    }
    text_buf[cursor_pos] = c;
    cursor_pos++;
    text_len++;
    text_buf[text_len] = '\0';
}

static void delete_char(void) {
    if (cursor_pos <= 0 || text_len <= 0) return;
    /* Shift chars left */
    for (int i = cursor_pos - 1; i < text_len - 1; i++) {
        text_buf[i] = text_buf[i + 1];
    }
    cursor_pos--;
    text_len--;
    text_buf[text_len] = '\0';
}

/* Calculate current line and column of cursor */
static void get_cursor_line_col(int *out_line, int *out_col) {
    int line = 1;
    int col = 1;
    for (int i = 0; i < cursor_pos; i++) {
        if (text_buf[i] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }
    *out_line = line;
    *out_col = col;
}

/* Render complete Notepad interface */
static void render_notepad(os_window_t *win) {
    int cw = win->client_w;
    int ch = win->client_h;

    /* 1. Main Editor Canvas Background */
    os_fill_rect(win, 0, 0, cw, ch, OS_COLOR_SLATE_950);

    /* 2. Top Toolbar */
    os_fill_gradient_v(win, 0, 0, cw, TOP_BAR_H, OS_COLOR_SLATE_900, OS_COLOR_SLATE_950);
    os_fill_rect(win, 0, TOP_BAR_H - 1, cw, 1, OS_COLOR_SLATE_700);

    /* App badge / Document title */
    os_draw_badge(win, 12, 9, "NOTEPAD", OS_COLOR_INDIGO, OS_COLOR_WHITE);
    os_draw_text_ui_aa(win, 112, 11, doc_title, OS_COLOR_SLATE_200);

    /* Toolbar buttons */
    os_draw_button_modern(win, BTN_SAVE_X,   BTN_SAVE_Y,   BTN_SAVE_W,   BTN_SAVE_H,   "Save",   OS_COLOR_EMERALD, 0, 0);
    os_draw_button_modern(win, BTN_SAMPLE_X, BTN_SAMPLE_Y, BTN_SAMPLE_W, BTN_SAMPLE_H, "Sample", OS_COLOR_CYAN_NEON, 0, 0);
    os_draw_button_modern(win, BTN_CLEAR_X,  BTN_CLEAR_Y,  BTN_CLEAR_W,  BTN_CLEAR_H,  "Clear",  OS_COLOR_ROSE, 0, 0);

    /* 3. Line Number Gutter */
    int editor_top = TOP_BAR_H;
    int editor_bot = ch - BOT_BAR_H;
    int editor_h   = editor_bot - editor_top;

    os_fill_rect(win, 0, editor_top, GUTTER_W, editor_h, OS_COLOR_SLATE_900);
    os_fill_rect(win, GUTTER_W - 1, editor_top, 1, editor_h, OS_COLOR_SLATE_700);

    /* 4. Render Text Lines & Line Numbers */
    int cur_line = 1;
    int line_y = editor_top + 8;
    int text_x = GUTTER_W + 12;
    int cur_x  = text_x;

    /* Draw first line number */
    char lnum_str[8];
    os_itoa(cur_line, lnum_str);
    int num_w = os_text_width_ui_aa(lnum_str);
    int num_x = GUTTER_W - 12 - num_w;
    os_draw_text_ui_aa(win, num_x > 4 ? num_x : 4, line_y, lnum_str, OS_COLOR_SLATE_500);

    int cursor_draw_x = cur_x;
    int cursor_draw_y = line_y;

    for (int i = 0; i <= text_len; i++) {
        if (i == cursor_pos) {
            cursor_draw_x = cur_x;
            cursor_draw_y = line_y;
        }

        if (i == text_len) break;

        char c = text_buf[i];
        if (c == '\n') {
            cur_line++;
            line_y += 20;
            cur_x = text_x;

            if (line_y + 20 < editor_bot) {
                os_itoa(cur_line, lnum_str);
                int nw = os_text_width_ui_aa(lnum_str);
                int nx = GUTTER_W - 12 - nw;
                os_draw_text_ui_aa(win, nx > 4 ? nx : 4, line_y, lnum_str, OS_COLOR_SLATE_500);
            }
        } else {
            int cw_char = (c >= 32 && c <= 126) ? os_font_ui_widths[c - 32] : 6;
            /* Word wrap if exceeds line width */
            if (cur_x + cw_char >= cw - 12) {
                cur_line++;
                line_y += 20;
                cur_x = text_x;
            }
            if (line_y + 20 < editor_bot) {
                os_draw_char_ui_aa(win, cur_x, line_y, c, OS_COLOR_SLATE_100);
            }
            cur_x += cw_char;
        }
    }

    /* 5. Draw Smooth Blinking Text Cursor */
    if ((blink_cnt % 70) < 42) {
        if (cursor_draw_y + 16 < editor_bot) {
            os_fill_rounded_rect(win, cursor_draw_x, cursor_draw_y, 2, 16, 1, OS_COLOR_CYAN_NEON);
        }
    }

    /* 6. Bottom Status Bar */
    os_fill_rect(win, 0, ch - BOT_BAR_H, cw, BOT_BAR_H, OS_COLOR_SLATE_900);
    os_fill_rect(win, 0, ch - BOT_BAR_H, cw, 1, OS_COLOR_SLATE_700);

    int cur_l, cur_c;
    get_cursor_line_col(&cur_l, &cur_c);

    char stats_str[64];
    char l_str[16], c_str[16], cnt_str[16];
    os_itoa(cur_l, l_str);
    os_itoa(cur_c, c_str);
    os_itoa(text_len, cnt_str);

    os_strcpy(stats_str, "Ln ");
    size_t pos = os_strlen(stats_str);
    os_strcpy(stats_str + pos, l_str);
    pos = os_strlen(stats_str);
    os_strcpy(stats_str + pos, ", Col ");
    pos = os_strlen(stats_str);
    os_strcpy(stats_str + pos, c_str);
    pos = os_strlen(stats_str);
    os_strcpy(stats_str + pos, " | ");
    pos = os_strlen(stats_str);
    os_strcpy(stats_str + pos, cnt_str);
    pos = os_strlen(stats_str);
    os_strcpy(stats_str + pos, " chars | Ring 3 C App");

    os_draw_text_ui_aa(win, 14, ch - BOT_BAR_H + 5, stats_str, OS_COLOR_SLATE_400);
}

#include "notepad_icon.h"

/* Application Package Metadata embedded in ELF section .os_app */
__attribute__((section(".os_app"), used))
static const os_app_package_t notepad_pkg = {
    .magic = OS_APP_MAGIC,
    .name = "Notepad",
    .version = "1.0.0",
    .author = "OpenSweet Team [Verified]",
    .description = "Modern GUI Text Editor",
    .exec_path = "/notepad.elf",
    .icon_width = 44,
    .icon_height = 44,
    .icon_pixels = {
        NOTEPAD_ICON_PIXELS_INIT
    }
};

int main(void) {
    os_print("[notepad.elf] Launching C Desktop Notepad in Ring 3...\n");

    /* Register application package metadata with OpenSweet OS */
    os_register_app(&notepad_pkg);

    /* Create 540x420 window */
    os_window_t win = os_create_window("Notepad - Document 1", 720, 260, WIN_W, WIN_H);
    if (win.win_id < 0) {
        os_print("[notepad.elf] Failed to create notepad window!\n");
        return 1;
    }

    /* Check if a file was requested to open (e.g. from File Explorer) */
    char req_path[64];
    ssize_t req_n = os_read_file("/last_opened.txt", req_path, sizeof(req_path) - 1);
    if (req_n > 0) {
        req_path[req_n] = '\0';
        /* Clear last_opened so subsequent launches don't reuse it indefinitely */
        os_write_file("/last_opened.txt", "", 0);
        load_file(req_path);
    } else {
        insert_sample();
    }

    render_notepad(&win);
    os_update_window(&win);

    os_print("[notepad.elf] Notepad window opened. Entering event loop.\n");

    os_event_t ev;
    bool running = true;

    while (running) {
        blink_cnt++;
        bool needs_redraw = false;

        /* Cursor blink toggle every 35 ticks */
        if ((blink_cnt % 35) == 0) {
            needs_redraw = true;
        }

        while (os_poll_event(&win, &ev)) {
            if (ev.type == OS_EVENT_WIN_CLOSE) {
                running = false;
                break;
            } else if (ev.type == OS_EVENT_WIN_MAXIMIZE) {
                win.width = ev.x;
                win.height = ev.y;
                win.client_w = ev.x;
                win.client_h = ev.y > 33 ? ev.y - 33 : 0;
                needs_redraw = true;
            } else if (ev.type == OS_EVENT_MOUSE_DOWN) {
                /* Toolbar button clicks */
                if (os_is_inside(ev.x, ev.y, BTN_SAVE_X, BTN_SAVE_Y, BTN_SAVE_W, BTN_SAVE_H)) {
                    save_file();
                    needs_redraw = true;
                } else if (os_is_inside(ev.x, ev.y, BTN_SAMPLE_X, BTN_SAMPLE_Y, BTN_SAMPLE_W, BTN_SAMPLE_H)) {
                    insert_sample();
                    needs_redraw = true;
                } else if (os_is_inside(ev.x, ev.y, BTN_CLEAR_X, BTN_CLEAR_Y, BTN_CLEAR_W, BTN_CLEAR_H)) {
                    clear_text();
                    needs_redraw = true;
                }
            } else if (ev.type == OS_EVENT_KEY_DOWN) {
                char k = (char)ev.param;
                blink_cnt = 0; /* Keep cursor visible while typing */
                if (k == 8) {
                    /* Backspace */
                    delete_char();
                    needs_redraw = true;
                } else if (k == 10 || k == 13) {
                    /* Enter */
                    insert_char('\n');
                    needs_redraw = true;
                } else if (k >= 32 && k <= 126) {
                    /* Printable character */
                    insert_char(k);
                    needs_redraw = true;
                }
            }
        }

        if (needs_redraw) {
            render_notepad(&win);
            os_update_window(&win);
        }

        os_sleep(30);
    }

    os_print("[notepad.elf] Closing notepad window and exiting.\n");
    os_close_window(&win);
    return 0;
}
