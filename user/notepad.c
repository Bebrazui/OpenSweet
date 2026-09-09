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

#define GUTTER_W 42
#define TOP_BAR_H 34
#define BOT_BAR_H 22

static char text_buf[MAX_TEXT];
static int  text_len = 0;
static int  cursor_pos = 0;
static int  blink_cnt = 0;

/* Toolbar buttons */
#define BTN_CLEAR_X 450
#define BTN_CLEAR_Y 6
#define BTN_CLEAR_W 72
#define BTN_CLEAR_H 22

#define BTN_SAMPLE_X 370
#define BTN_SAMPLE_Y 6
#define BTN_SAMPLE_W 72
#define BTN_SAMPLE_H 22

static void insert_sample(void) {
    const char *sample = 
        "Welcome to OpenSweet Notepad!\n"
        "Written in pure C using OpenSweet SDK.\n"
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
    os_fill_rect(win, 0, 0, cw, TOP_BAR_H, OS_COLOR_SLATE_900);
    os_fill_rect(win, 0, TOP_BAR_H - 1, cw, 1, OS_COLOR_SLATE_700);

    /* App icon / Document title */
    os_draw_text(win, 12, 12, "NOTEPAD", OS_COLOR_INDIGO_LT);
    os_draw_text(win, 80, 12, "Document 1.txt", OS_COLOR_SLATE_300);

    /* Toolbar buttons */
    os_draw_button(win, BTN_SAMPLE_X, BTN_SAMPLE_Y, BTN_SAMPLE_W, BTN_SAMPLE_H, "Sample", OS_COLOR_SLATE_800, OS_COLOR_SLATE_200, 0);
    os_draw_button(win, BTN_CLEAR_X,  BTN_CLEAR_Y,  BTN_CLEAR_W,  BTN_CLEAR_H,  "Clear",  OS_COLOR_SLATE_800, OS_COLOR_SLATE_200, 0);

    /* 3. Line Number Gutter */
    int editor_top = TOP_BAR_H;
    int editor_bot = ch - BOT_BAR_H;
    int editor_h   = editor_bot - editor_top;

    os_fill_rect(win, 0, editor_top, GUTTER_W, editor_h, OS_COLOR_SLATE_900);
    os_fill_rect(win, GUTTER_W - 1, editor_top, 1, editor_h, OS_COLOR_SLATE_700);

    /* 4. Render Text Lines & Line Numbers */
    int cur_line = 1;
    int line_y = editor_top + 8;
    int text_x = GUTTER_W + 10;
    int cur_x  = text_x;

    /* Draw first line number */
    char lnum_str[8];
    os_itoa(cur_line, lnum_str);
    int num_x = GUTTER_W - 12 - (int)os_strlen(lnum_str) * 8;
    os_draw_text(win, num_x > 4 ? num_x : 4, line_y, lnum_str, OS_COLOR_SLATE_500);

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
            line_y += 14;
            cur_x = text_x;

            if (line_y + 14 < editor_bot) {
                os_itoa(cur_line, lnum_str);
                int nx = GUTTER_W - 12 - (int)os_strlen(lnum_str) * 8;
                os_draw_text(win, nx > 4 ? nx : 4, line_y, lnum_str, OS_COLOR_SLATE_500);
            }
        } else {
            /* Word wrap if exceeds line width */
            if (cur_x + 10 >= cw - 12) {
                cur_line++;
                line_y += 14;
                cur_x = text_x;
            }
            if (line_y + 14 < editor_bot) {
                os_draw_char(win, cur_x, line_y, c, OS_COLOR_SLATE_100);
            }
            cur_x += 8;
        }
    }

    /* 5. Draw Blinking Text Cursor */
    if ((blink_cnt % 30) < 18) {
        if (cursor_draw_y + 12 < editor_bot) {
            os_fill_rect(win, cursor_draw_x, cursor_draw_y, 2, 10, OS_COLOR_CYAN);
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

    os_draw_text(win, 12, ch - BOT_BAR_H + 6, stats_str, OS_COLOR_SLATE_400);
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
    os_window_t win = os_create_window("Notepad - Document 1", 280, 140, WIN_W, WIN_H);
    if (win.win_id < 0) {
        os_print("[notepad.elf] Failed to create notepad window!\n");
        return 1;
    }

    insert_sample();

    render_notepad(&win);
    os_update_window(&win);

    os_print("[notepad.elf] Notepad window opened. Entering event loop.\n");

    os_event_t ev;
    bool running = true;

    while (running) {
        blink_cnt++;
        bool needs_redraw = false;

        /* Cursor blink toggle every 15 ticks */
        if ((blink_cnt % 15) == 0) {
            needs_redraw = true;
        }

        while (os_poll_event(&win, &ev)) {
            if (ev.type == OS_EVENT_WIN_CLOSE) {
                running = false;
                break;
            } else if (ev.type == OS_EVENT_MOUSE_DOWN) {
                /* Toolbar button clicks */
                if (os_is_inside(ev.x, ev.y, BTN_SAMPLE_X, BTN_SAMPLE_Y, BTN_SAMPLE_W, BTN_SAMPLE_H)) {
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

        os_sleep(16);
    }

    os_print("[notepad.elf] Closing notepad window and exiting.\n");
    os_close_window(&win);
    return 0;
}
