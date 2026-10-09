/* =============================================================================
 * OpenSweet OS - Native C Desktop File Explorer (user/files.c)
 * Ring 3 graphical file manager written in standard C using OpenSweet SDK.
 * Features:
 *   - Standalone Ring 3 ELF application with embedded .os_app metadata and icon
 *   - Real ext4 directory reading via os_read_dir()
 *   - Modern Fluent/Glass UI: breadcrumbs, cards, color-coded badges, selection
 *   - Interactive row selection, Refresh button, and context action buttons
 *   - Direct app launching (os_spawn) for ELF and text documents
 * ============================================================================= */

#include "opensweet.h"
#include "files_icon.h"

#define WIN_W 640
#define WIN_H 480

#define TOP_H     48
#define TABLE_HDR 28
#define ROW_H     34
#define BOT_H     56

__attribute__((section(".os_app"), used))
static const os_app_package_t files_pkg = {
    .magic = OS_APP_MAGIC,
    .name = "Files",
    .version = "1.0.0",
    .author = "OpenSweet Team",
    .description = "ext4 File Explorer",
    .exec_path = "/files.elf",
    .icon_width = 44,
    .icon_height = 44,
    .icon_pixels = FILES_ICON_PIXELS_INIT
};

static os_dirent_t entries[32];
static int         entry_count = 0;
static int         selected_idx = 0;
static int         hovered_row = -1;

/* Format file size into human readable string */
static void format_size(uint64_t bytes, char *out) {
    if (bytes >= 1048576) {
        char num[16];
        os_itoa(bytes / 1048576, num);
        os_strcpy(out, num);
        os_strcpy(out + os_strlen(out), ".");
        char frac[8];
        os_itoa(((bytes % 1048576) * 10) / 1048576, frac);
        os_strcpy(out + os_strlen(out), frac);
        os_strcpy(out + os_strlen(out), " MB");
    } else if (bytes >= 1024) {
        char num[16];
        os_itoa(bytes / 1024, num);
        os_strcpy(out, num);
        os_strcpy(out + os_strlen(out), ".");
        char frac[8];
        os_itoa(((bytes % 1024) * 10) / 1024, frac);
        os_strcpy(out + os_strlen(out), frac);
        os_strcpy(out + os_strlen(out), " KB");
    } else {
        char num[16];
        os_itoa((int64_t)bytes, num);
        os_strcpy(out, num);
        os_strcpy(out + os_strlen(out), " B");
    }
}

static void refresh_files(void) {
    entry_count = os_read_dir(entries, 32);
    if (entry_count < 0) entry_count = 0;
    if (selected_idx >= entry_count && entry_count > 0) {
        selected_idx = 0;
    }
}

/* Render complete File Explorer interface */
static void render_explorer(os_window_t *win) {
    int cw = win->client_w;
    int ch = win->client_h;

    /* 1. Canvas Background */
    os_fill_rect(win, 0, 0, cw, ch, OS_COLOR_OBSIDIAN);

    /* 2. Top Header & Breadcrumb Bar */
    os_fill_gradient_v(win, 0, 0, cw, TOP_H, OS_COLOR_SLATE_900, OS_COLOR_SLATE_950);
    os_fill_rect(win, 0, TOP_H - 1, cw, 1, OS_COLOR_SLATE_700);

    /* Breadcrumb Card */
    os_draw_card(win, 12, 8, 240, 32, OS_COLOR_SLATE_800, OS_COLOR_SLATE_600, 6);
    os_draw_badge(win, 16, 14, "ROOT", OS_COLOR_AMBER, OS_COLOR_BLACK);
    os_draw_text_ui_aa(win, 82, 16, "/ rootfs (ext4)", OS_COLOR_WHITE);

    /* Storage Status Badge */
    os_draw_badge(win, 260, 14, "ext4 | Primary ATA", OS_COLOR_EMERALD_DK, OS_COLOR_WHITE);

    /* Refresh Button */
    os_draw_button_modern(win, cw - 96, 9, 84, 30, "Refresh", OS_COLOR_CYAN_NEON, 0, 0);

    /* 3. Table Column Header Bar */
    int table_top = TOP_H;
    os_fill_rect(win, 0, table_top, cw, TABLE_HDR, OS_COLOR_SLATE_900);
    os_fill_rect(win, 0, table_top + TABLE_HDR - 1, cw, 1, OS_COLOR_SLATE_700);

    os_draw_text_ui_aa(win, 20,  table_top + 6, "TYPE",  OS_COLOR_SLATE_400);
    os_draw_text_ui_aa(win, 114, table_top + 6, "NAME",  OS_COLOR_SLATE_400);
    os_draw_text_ui_aa(win, 370, table_top + 6, "SIZE",  OS_COLOR_SLATE_400);
    os_draw_text_ui_aa(win, 500, table_top + 6, "INODE", OS_COLOR_SLATE_400);

    /* 4. Table Rows with Vector Icons & Anti-Aliased Typography */
    int y = table_top + TABLE_HDR + 4;
    for (int i = 0; i < entry_count; i++) {
        if (y + ROW_H > ch - BOT_H) break;

        int is_selected = (i == selected_idx);
        int is_hovered  = (i == hovered_row);

        if (is_selected) {
            os_draw_card(win, 8, y, cw - 16, ROW_H - 2, OS_COLOR_SLATE_800, OS_COLOR_INDIGO_LT, 6);
        } else if (is_hovered) {
            os_draw_card(win, 8, y, cw - 16, ROW_H - 2, OS_COLOR_SLATE_850, OS_COLOR_SLATE_700, 6);
        }

        /* Type Detection */
        int is_dir = (entries[i].type == 2);
        int nlen = (int)os_strlen(entries[i].name);
        int is_elf = (nlen >= 4 && entries[i].name[nlen-4] == '.' && entries[i].name[nlen-3] == 'e');
        int is_png = (nlen >= 4 && entries[i].name[nlen-4] == '.' && entries[i].name[nlen-3] == 'p');

        /* Vector Icon & Pill Badge */
        if (is_dir) {
            os_draw_icon_folder(win, 18, y + 8);
            os_draw_badge(win, 48, y + 7, "DIR", OS_COLOR_AMBER, OS_COLOR_BLACK);
        } else if (is_elf) {
            os_draw_icon_file(win, 20, y + 7, 1);
            os_draw_badge(win, 48, y + 7, "APP", OS_COLOR_INDIGO_LT, OS_COLOR_WHITE);
        } else if (is_png) {
            os_draw_icon_file(win, 20, y + 7, 0);
            os_draw_badge(win, 48, y + 7, "IMG", OS_COLOR_SKY, OS_COLOR_WHITE);
        } else {
            os_draw_icon_file(win, 20, y + 7, 0);
            os_draw_badge(win, 48, y + 7, "TXT", OS_COLOR_EMERALD, OS_COLOR_WHITE);
        }

        /* Name */
        uint32_t name_col = is_dir ? OS_COLOR_AMBER : (is_elf ? OS_COLOR_CYAN_NEON : OS_COLOR_WHITE);
        os_draw_text_ui_aa(win, 114, y + 9, entries[i].name, name_col);

        /* Size */
        char sz_str[32];
        format_size(entries[i].size, sz_str);
        os_draw_text_ui_aa(win, 370, y + 9, sz_str, OS_COLOR_SLATE_300);

        /* Inode */
        char in_str[32], in_num[16];
        os_itoa(entries[i].inode, in_num);
        os_strcpy(in_str, "#");
        os_strcpy(in_str + os_strlen(in_str), in_num);
        os_draw_text_ui_aa(win, 500, y + 9, in_str, OS_COLOR_SLATE_400);

        y += ROW_H;
    }

    /* 5. Bottom Details Action Panel */
    int bot_y = ch - BOT_H;
    os_fill_gradient_v(win, 0, bot_y, cw, BOT_H, OS_COLOR_SLATE_900, OS_COLOR_SLATE_950);
    os_fill_rect(win, 0, bot_y, cw, 1, OS_COLOR_SLATE_700);

    if (entry_count > 0 && selected_idx < entry_count) {
        /* Selected item details */
        char sel_info[128];
        char sz[32];
        format_size(entries[selected_idx].size, sz);
        sel_info[0] = '\0';
        os_strncpy(sel_info, "Selected: ", sizeof(sel_info) - 1);
        size_t cur = os_strlen(sel_info);
        os_strncpy(sel_info + cur, entries[selected_idx].name, sizeof(sel_info) - cur - 1);
        cur = os_strlen(sel_info);
        os_strncpy(sel_info + cur, " (", sizeof(sel_info) - cur - 1);
        cur = os_strlen(sel_info);
        os_strncpy(sel_info + cur, sz, sizeof(sel_info) - cur - 1);
        cur = os_strlen(sel_info);
        os_strncpy(sel_info + cur, ")", sizeof(sel_info) - cur - 1);
        sel_info[sizeof(sel_info) - 1] = '\0';
        os_draw_text_ui_aa(win, 18, bot_y + 19, sel_info, OS_COLOR_SLATE_200);

        /* Action button on bottom right */
        int is_elf = (os_strlen(entries[selected_idx].name) > 4 && 
                      entries[selected_idx].name[os_strlen(entries[selected_idx].name)-1] == 'f');
        if (is_elf) {
            os_draw_button_modern(win, cw - 150, bot_y + 11, 136, 34, "Run App", OS_COLOR_CYAN_NEON, 0, 0);
        } else {
            os_draw_button_modern(win, cw - 150, bot_y + 11, 136, 34, "Open Note", OS_COLOR_EMERALD, 0, 0);
        }
    } else {
        os_draw_text_ui_aa(win, 18, bot_y + 19, "ext4 storage mounted and healthy. 0 items selected.", OS_COLOR_SLATE_400);
    }
}

int main(void) {
    os_register_app(&files_pkg);

    os_window_t win = os_create_window("File Explorer - ext4 Storage (/)", 160, 100, WIN_W, WIN_H);
    if (win.win_id < 0) return 1;

    refresh_files();

    render_explorer(&win);
    os_update_window(&win);

    os_event_t ev;
    while (1) {
        int has_event = os_poll_event(&win, &ev);
        if (has_event) {
            if (ev.type == OS_EVENT_WIN_CLOSE) {
                break;
            } else if (ev.type == OS_EVENT_WIN_MAXIMIZE) {
                win.width = ev.x;
                win.height = ev.y;
                win.client_w = ev.x;
                win.client_h = ev.y > 33 ? ev.y - 33 : 0;
            } else if (ev.type == OS_EVENT_MOUSE_MOVE) {
                /* Check hover row */
                int my = ev.y;
                int start_y = TOP_H + TABLE_HDR + 4;
                if (my >= start_y && my < win.client_h - BOT_H) {
                    hovered_row = (my - start_y) / ROW_H;
                    if (hovered_row >= entry_count) hovered_row = -1;
                } else {
                    hovered_row = -1;
                }
            } else if (ev.type == OS_EVENT_MOUSE_DOWN) {
                int mx = ev.x;
                int my = ev.y;

                /* 1. Refresh Button Click */
                if (mx >= win.client_w - 88 && mx <= win.client_w - 12 && my >= 10 && my <= 38) {
                    refresh_files();
                }
                /* 2. Action Button Click */
                else if (mx >= win.client_w - 150 && mx <= win.client_w - 14 && 
                         my >= win.client_h - BOT_H + 12 && my <= win.client_h - BOT_H + 44) {
                    if (entry_count > 0 && selected_idx < entry_count) {
                        char path[128];
                        path[0] = '/';
                        path[1] = '\0';
                        os_strncpy(path + 1, entries[selected_idx].name, sizeof(path) - 2);
                        path[sizeof(path) - 1] = '\0';
                        int is_elf = (os_strlen(entries[selected_idx].name) > 4 && 
                                      entries[selected_idx].name[os_strlen(entries[selected_idx].name)-1] == 'f');
                        if (is_elf) {
                            os_spawn(path);
                        } else {
                            /* Store selected file path so Notepad opens it automatically */
                            os_write_file("/last_opened.txt", path, os_strlen(path));
                            os_spawn("/notepad.elf");
                        }
                    }
                }
                /* 3. Row Selection Click */
                else {
                    int start_y = TOP_H + TABLE_HDR + 4;
                    if (my >= start_y && my < win.client_h - BOT_H) {
                        int r = (my - start_y) / ROW_H;
                        if (r >= 0 && r < entry_count) {
                            selected_idx = r;
                        }
                    }
                }
            }
            render_explorer(&win);
            os_update_window(&win);
        }

        os_sleep(25);
    }

    os_close_window(&win);
    return 0;
}
