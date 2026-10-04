/* =============================================================================
 * OpenSweet OS - Doomgeneric Platform Adapter (user/doomgeneric_opensweet.c)
 * ============================================================================= */

#include "opensweet.h"
#include "doomgeneric.h"
#include "doomkeys.h"

#define DOOM_W 640
#define DOOM_H 400

static os_window_t doom_win;

/* Key event queue */
#define KEYQUEUE_SIZE 128
static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static void addKeyToQueue(int pressed, unsigned char key) {
    if (key == 0) return;
    unsigned short keyData = ((pressed ? 1 : 0) << 8) | key;
    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex = (s_KeyQueueWriteIndex + 1) % KEYQUEUE_SIZE;
}

static unsigned char map_os_key(unsigned int keyCode) {
    switch (keyCode) {
        /* UI & Menu Controls */
        case 10:
        case 13: return KEY_ENTER;
        case 27: return KEY_ESCAPE;
        case 9:  return KEY_TAB;
        case 8:  return KEY_BACKSPACE;

        /* Cursor Arrows (OS_KEY_* from kernel or raw scancodes) */
        case OS_KEY_UP:
        case 0x48: return KEY_UPARROW;
        case OS_KEY_DOWN:
        case 0x50: return KEY_DOWNARROW;
        case OS_KEY_LEFT:
        case 0x4B: return KEY_LEFTARROW;
        case OS_KEY_RIGHT:
        case 0x4D: return KEY_RIGHTARROW;

        /* Movement: WASD */
        case 'w':
        case 'W': return KEY_UPARROW;
        case 's':
        case 'S': return KEY_DOWNARROW;
        case 'a':
        case 'A': return KEY_STRAFE_L;
        case 'd':
        case 'D': return KEY_STRAFE_R;

        /* Turn left with left hand */
        case 'q':
        case 'Q': return KEY_LEFTARROW;

        /* Actions: Use / Open Doors */
        case ' ':
        case 'e':
        case 'E': return KEY_USE;

        /* Actions: Fire / Attack */
        case OS_KEY_CTRL:
        case 0x1D:
        case 0x9D:
        case 'f':
        case 'F':
        case 'z':
        case 'Z':
        case 'c':
        case 'C': return KEY_FIRE;

        /* Strafe keys (alternative) */
        case ',':
        case '<': return KEY_STRAFE_L;
        case '.':
        case '>': return KEY_STRAFE_R;

        /* Run / Speed */
        case KEY_RSHIFT: return KEY_RSHIFT;

        /* Menu answers */
        case 'y':
        case 'Y': return 'y';
        case 'n':
        case 'N': return 'n';

        /* Screen sizing */
        case '=':
        case '+': return KEY_EQUALS;
        case '-':
        case '_': return KEY_MINUS;

        /* Weapons 1..7 */
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7': return (unsigned char)keyCode;

        default:
            if (keyCode >= 'A' && keyCode <= 'Z') return (unsigned char)(keyCode + 32);
            if (keyCode >= 32 && keyCode <= 126) return (unsigned char)keyCode;
            return 0;
    }
}

static int s_FireIsDown = 0;
static int s_FireDoomActive = 0;
static int s_FireHoldFrames = 0;

void DG_Init(void) {
    doom_win = os_create_window("DOOM (Shareware) - OpenSweet OS", 120, 80, DOOM_W, DOOM_H + 33);
}

void DG_DrawFrame(void) {
    if (!doom_win.canvas || doom_win.win_id < 0) return;

    /* DG_ScreenBuffer is 640x400 uint32 (ARGB) */
    uint32_t *src = (uint32_t*)DG_ScreenBuffer;
    uint32_t *dst = doom_win.canvas;
    
    int total_pixels = DOOM_W * (doom_win.client_h < DOOM_H ? doom_win.client_h : DOOM_H);
    for (int i = 0; i < total_pixels; i++) {
        dst[i] = src[i];
    }

    /* Process window events with native down & up event pipeline */
    os_event_t ev;
    while (os_poll_event(&doom_win, &ev)) {
        if (ev.type == OS_EVENT_WIN_CLOSE) {
            os_exit(0);
        } else if (ev.type == OS_EVENT_KEY_DOWN) {
            unsigned char dkey = map_os_key(ev.param);
            if (dkey == KEY_FIRE) {
                s_FireIsDown = 1;
                if (!s_FireDoomActive) {
                    s_FireDoomActive = 1;
                    s_FireHoldFrames = 0;
                    addKeyToQueue(1, KEY_FIRE);
                }
            } else if (dkey != 0) {
                addKeyToQueue(1, dkey);
            }
        } else if (ev.type == OS_EVENT_KEY_UP) {
            unsigned char dkey = map_os_key(ev.param);
            if (dkey == KEY_FIRE) {
                s_FireIsDown = 0;
            } else if (dkey != 0) {
                addKeyToQueue(0, dkey);
            }
        } else if (ev.type == OS_EVENT_MOUSE_DOWN) {
            s_FireIsDown = 1;
            if (!s_FireDoomActive) {
                s_FireDoomActive = 1;
                s_FireHoldFrames = 0;
                addKeyToQueue(1, KEY_FIRE);
            }
        } else if (ev.type == OS_EVENT_MOUSE_UP) {
            s_FireIsDown = 0;
        }
    }

    /* Guard: single tap fires cleanly for at least 2 frames and ceases immediately on release */
    if (s_FireDoomActive) {
        s_FireHoldFrames++;
        if (!s_FireIsDown && s_FireHoldFrames >= 2) {
            s_FireDoomActive = 0;
            s_FireHoldFrames = 0;
            addKeyToQueue(0, KEY_FIRE);
        }
    }

    os_update_window(&doom_win);
}

void DG_SleepMs(uint32_t ms) {
    os_sleep(ms);
}

uint32_t DG_GetTicksMs(void) {
    return os_uptime();
}

int DG_GetKey(int *pressed, unsigned char *doomKey) {
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) return 0;
    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex = (s_KeyQueueReadIndex + 1) % KEYQUEUE_SIZE;
    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;
    return 1;
}

void DG_SetWindowTitle(const char *title) {
    (void)title;
}

int main(int argc, char **argv) {
    char *doom_args[] = { "doom.elf", "-iwad", "/doom1.wad", "-mb", "8", NULL };
    doomgeneric_Create(5, doom_args);

    while (1) {
        doomgeneric_Tick();
    }
    return 0;
}
