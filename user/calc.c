/* =============================================================================
 * OpenSweet OS - Native C Desktop Calculator (user/calc.c)
 * Ring 3 graphical application written in standard C using OpenSweet C SDK.
 * Features:
 *   - 32bpp modern Slate/Indigo/Emerald styled user interface
 *   - Mouse click interactive buttons with pressed-state animations
 *   - Full keyboard typing support (0-9, +, -, *, /, Enter, Backspace, C)
 *   - Scaled 2x large font rendering for the calculator display
 *   - Addition, Subtraction, Multiplication, Integer Division & Sign inversion
 * ============================================================================= */

#include "opensweet.h"

#define WIN_W 320
#define WIN_H 420

typedef struct {
    int x, y, w, h;
    const char *label;
    uint32_t bg_color;
    uint32_t fg_color;
    int id;
} button_t;

enum {
    BTN_0 = 0, BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7, BTN_8, BTN_9,
    BTN_DOT, BTN_EQUAL,
    BTN_PLUS, BTN_MINUS, BTN_MUL, BTN_DIV,
    BTN_CLEAR, BTN_NEG, BTN_PERCENT
};

static button_t buttons[] = {
    /* Row 0 */
    { 16, 100, 64, 48, "C",   OS_COLOR_ROSE,      OS_COLOR_WHITE,     BTN_CLEAR },
    { 90, 100, 64, 48, "+/-", OS_COLOR_SLATE_700, OS_COLOR_SLATE_200, BTN_NEG },
    { 164, 100, 64, 48, "%",  OS_COLOR_SLATE_700, OS_COLOR_SLATE_200, BTN_PERCENT },
    { 238, 100, 64, 48, "/",  OS_COLOR_INDIGO,    OS_COLOR_WHITE,     BTN_DIV },

    /* Row 1 */
    { 16, 158, 64, 48, "7",   OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_7 },
    { 90, 158, 64, 48, "8",   OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_8 },
    { 164, 158, 64, 48, "9",  OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_9 },
    { 238, 158, 64, 48, "*",  OS_COLOR_INDIGO,    OS_COLOR_WHITE,     BTN_MUL },

    /* Row 2 */
    { 16, 216, 64, 48, "4",   OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_4 },
    { 90, 216, 64, 48, "5",   OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_5 },
    { 164, 216, 64, 48, "6",  OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_6 },
    { 238, 216, 64, 48, "-",  OS_COLOR_INDIGO,    OS_COLOR_WHITE,     BTN_MINUS },

    /* Row 3 */
    { 16, 274, 64, 48, "1",   OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_1 },
    { 90, 274, 64, 48, "2",   OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_2 },
    { 164, 274, 64, 48, "3",  OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_3 },
    { 238, 274, 64, 48, "+",  OS_COLOR_INDIGO,    OS_COLOR_WHITE,     BTN_PLUS },

    /* Row 4 */
    { 16, 332, 138, 48, "0",  OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_0 },
    { 164, 332, 64, 48, ".",  OS_COLOR_SLATE_800, OS_COLOR_WHITE,     BTN_DOT },
    { 238, 332, 64, 48, "=",  OS_COLOR_EMERALD,   OS_COLOR_WHITE,     BTN_EQUAL },
};

#define BUTTON_COUNT (sizeof(buttons) / sizeof(buttons[0]))

/* Calculator State */
static int64_t current_val = 0;
static int64_t prev_val = 0;
static char active_op = 0;
static bool has_prev = false;
static bool reset_input = false;
static bool has_error = false;
static int active_pressed_btn = -1;

/* Scaled text renderer for large display digits (2x) */
static void draw_scaled_char(os_window_t *win, int x, int y, char c, uint32_t color, int scale) {
    if (!win || !win->canvas) return;
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *glyph = os_font8x8[c - 32];
    int cw = win->client_w;
    int ch = win->client_h;

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                for (int sy = 0; sy < scale; sy++) {
                    int py = y + row * scale + sy;
                    if (py < 0 || py >= ch) continue;
                    uint32_t *prow = win->canvas + py * cw;
                    for (int sx = 0; sx < scale; sx++) {
                        int px = x + col * scale + sx;
                        if (px >= 0 && px < cw) {
                            prow[px] = color;
                        }
                    }
                }
            }
        }
    }
}

static void draw_scaled_text(os_window_t *win, int x, int y, const char *str, uint32_t color, int scale) {
    if (!win || !str) return;
    int cur_x = x;
    while (*str) {
        draw_scaled_char(win, cur_x, y, *str, color, scale);
        cur_x += 8 * scale;
        str++;
    }
}

/* Redraw display screen and buttons */
static void render_calc(os_window_t *win) {
    /* 1. Background Card */
    os_fill_rect(win, 0, 0, win->client_w, win->client_h, OS_COLOR_SLATE_900);

    /* 2. Top Display Bezel */
    os_fill_rect(win, 16, 16, 286, 68, OS_COLOR_SLATE_950);
    os_draw_rect(win, 16, 16, 286, 68, OS_COLOR_SLATE_700);

    /* 2b. Secondary Expression History line */
    char hist_str[32];
    if (has_prev && active_op) {
        char num_str[24];
        os_itoa(prev_val, num_str);
        os_strcpy(hist_str, num_str);
        size_t len = os_strlen(hist_str);
        hist_str[len] = ' ';
        hist_str[len + 1] = active_op;
        hist_str[len + 2] = '\0';
    } else {
        hist_str[0] = '\0';
    }
    int hist_x = 290 - (int)os_strlen(hist_str) * 8;
    os_draw_text(win, hist_x, 24, hist_str, OS_COLOR_SLATE_400);

    /* 2c. Main Big Readout (scale 2x) */
    char main_str[32];
    if (has_error) {
        os_strcpy(main_str, "ERROR");
    } else {
        os_itoa(current_val, main_str);
    }
    int main_len = (int)os_strlen(main_str);
    int readout_x = 290 - main_len * 16;
    if (readout_x < 24) readout_x = 24;
    draw_scaled_text(win, readout_x, 46, main_str, has_error ? OS_COLOR_ROSE : OS_COLOR_WHITE, 2);

    /* 3. Render Buttons */
    for (size_t i = 0; i < BUTTON_COUNT; i++) {
        button_t *b = &buttons[i];
        int is_pressed = (active_pressed_btn == (int)i);
        os_draw_button(win, b->x, b->y, b->w, b->h, b->label, b->bg_color, b->fg_color, is_pressed);
    }
}

/* Execute arithmetic calculation */
static void execute_op(void) {
    if (!has_prev || !active_op) return;
    switch (active_op) {
        case '+':
            current_val = prev_val + current_val;
            break;
        case '-':
            current_val = prev_val - current_val;
            break;
        case '*':
            current_val = prev_val * current_val;
            break;
        case '/':
            if (current_val == 0) {
                has_error = true;
                current_val = 0;
            } else {
                current_val = prev_val / current_val;
            }
            break;
    }
    has_prev = false;
    active_op = 0;
    reset_input = true;
}

/* Button click dispatcher */
static void handle_button_action(int btn_id) {
    has_error = false;

    if (btn_id >= BTN_0 && btn_id <= BTN_9) {
        int digit = btn_id - BTN_0;
        if (reset_input) {
            current_val = digit;
            reset_input = false;
        } else {
            /* Max 14 digits to avoid overflow */
            if (current_val < 999999999999LL && current_val > -999999999999LL) {
                current_val = current_val * 10 + digit;
            }
        }
    } else if (btn_id == BTN_CLEAR) {
        current_val = 0;
        prev_val = 0;
        active_op = 0;
        has_prev = false;
        reset_input = false;
    } else if (btn_id == BTN_NEG) {
        current_val = -current_val;
    } else if (btn_id == BTN_PERCENT) {
        current_val = current_val / 100;
    } else if (btn_id == BTN_PLUS || btn_id == BTN_MINUS || btn_id == BTN_MUL || btn_id == BTN_DIV) {
        if (has_prev) {
            execute_op();
        }
        prev_val = current_val;
        has_prev = true;
        reset_input = true;
        if (btn_id == BTN_PLUS)  active_op = '+';
        if (btn_id == BTN_MINUS) active_op = '-';
        if (btn_id == BTN_MUL)   active_op = '*';
        if (btn_id == BTN_DIV)   active_op = '/';
    } else if (btn_id == BTN_EQUAL) {
        execute_op();
    }
}

int main(void) {
    os_print("[calc.elf] Launching C Desktop Calculator in Ring 3...\n");

    /* Create 320x420 desktop window centered */
    os_window_t win = os_create_window("Calculator", 400, 180, WIN_W, WIN_H);
    if (win.win_id < 0) {
        os_print("[calc.elf] Failed to create calculator window!\n");
        return 1;
    }

    render_calc(&win);
    os_update_window(&win);

    os_print("[calc.elf] Calculator window opened. Entering event loop.\n");

    os_event_t ev;
    bool running = true;

    while (running) {
        int has_event = os_poll_event(&win, &ev);
        if (has_event) {
            if (ev.type == OS_EVENT_WIN_CLOSE) {
                running = false;
            } else if (ev.type == OS_EVENT_MOUSE_DOWN) {
                /* Hit test buttons */
                for (size_t i = 0; i < BUTTON_COUNT; i++) {
                    button_t *b = &buttons[i];
                    if (os_is_inside(ev.x, ev.y, b->x, b->y, b->w, b->h)) {
                        active_pressed_btn = (int)i;
                        handle_button_action(b->id);
                        render_calc(&win);
                        os_update_window(&win);
                        os_sleep(60); /* Click press animation */
                        active_pressed_btn = -1;
                        render_calc(&win);
                        os_update_window(&win);
                        break;
                    }
                }
            } else if (ev.type == OS_EVENT_KEY_DOWN) {
                /* Keyboard typing support */
                char k = (char)ev.param;
                if (k >= '0' && k <= '9') {
                    handle_button_action(BTN_0 + (k - '0'));
                } else if (k == '+' || k == '-' || k == '*' || k == '/') {
                    if (k == '+') handle_button_action(BTN_PLUS);
                    if (k == '-') handle_button_action(BTN_MINUS);
                    if (k == '*') handle_button_action(BTN_MUL);
                    if (k == '/') handle_button_action(BTN_DIV);
                } else if (k == '=' || k == 10 || k == 13) {
                    handle_button_action(BTN_EQUAL);
                } else if (k == 'c' || k == 'C' || k == 8) {
                    handle_button_action(BTN_CLEAR);
                }
                render_calc(&win);
                os_update_window(&win);
            }
        }
        os_sleep(16);
    }

    os_print("[calc.elf] Closing calculator window and exiting.\n");
    os_close_window(&win);
    return 0;
}
