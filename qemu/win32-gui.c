/*
 * Built-in Win32 GUI for the UV-K5 V3 emulator.
 *
 * Replaces QEMU's default main() via the qemu_main function pointer.
 * Displays the LCD framebuffer in a window and maps keyboard input to
 * keypad keys.  The firmware is loaded automatically from the same
 * directory as the executable.
 *
 * This code is licensed under the GPL version 2 or later.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for QEMU internals we call. */
extern void qemu_init(int argc, char **argv);
extern int  qemu_main_loop(void);
extern void qemu_cleanup(int status);
extern void bql_lock(void);
extern void bql_unlock(void);

/* Forward declarations from py32f071.c (GUI glue layer). */
#define UVK5_LCD_WIDTH  128
#define UVK5_LCD_HEIGHT 64
extern int  uvk5_get_framebuffer(unsigned char *out, int bufsize);
extern int  uvk5_is_lcd_dirty(void);
extern void uvk5_key_press(const char *name);
extern void uvk5_key_release(void);

/* ----- LCD pixel decode (ST7565 layout: column-major, LSB at top) ----- */

static void decode_lcd(const unsigned char *raw, unsigned int *pixels)
{
    /* raw = 896 bytes gFrameBuffer + 128 bytes gStatusLine */
    for (int page = 0; page < 8; page++) {
        const unsigned char *src = (page == 0) ? raw + 896  /* status line */
                                               : raw + (page - 1) * UVK5_LCD_WIDTH;
        for (int col = 0; col < UVK5_LCD_WIDTH; col++) {
            unsigned char byte = src[col];
            for (int bit = 0; bit < 8; bit++) {
                int y = page * 8 + bit;
                if (y >= UVK5_LCD_HEIGHT) break;
                pixels[y * UVK5_LCD_WIDTH + col] =
                    (byte & (1 << bit)) ? 0x000000 : 0xFFFFFF;
            }
        }
    }
}

/* ----- Key mapping ----- */

static void handle_keydown(int vk, int repeat)
{
    if (repeat) return;  /* ignore key-repeat */

    const char *name = NULL;
    switch (vk) {
    case VK_UP:    name = "UP";    break;
    case VK_DOWN:  name = "DOWN";  break;
    case VK_LEFT:  name = "LEFT";  break;
    case VK_RIGHT: name = "RIGHT"; break;
    case VK_RETURN:name = "MENU";  break;
    case VK_ESCAPE:name = "EXIT";  break;
    case VK_SPACE: name = "SIDE1"; break;
    case VK_SHIFT: name = "SIDE2"; break;
    case 'F': case 'f': name = "F";    break;
    case 'S': case 's': name = "STAR"; break;
    case '0': name = "0"; break;
    case '1': name = "1"; break;
    case '2': name = "2"; break;
    case '3': name = "3"; break;
    case '4': name = "4"; break;
    case '5': name = "5"; break;
    case '6': name = "6"; break;
    case '7': name = "7"; break;
    case '8': name = "8"; break;
    case '9': name = "9"; break;
    }
    if (name) {
        uvk5_key_press(name);
    }
}

static void handle_keyup(int vk)
{
    /* Release on any key-up of a mapped key. */
    switch (vk) {
    case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
    case VK_RETURN: case VK_ESCAPE: case VK_SPACE: case VK_SHIFT:
    case 'F': case 'f': case 'S': case 's':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        uvk5_key_release();
        break;
    }
}

/* ----- Window class and rendering ----- */

#define LCD_SCALE    4
#define LCD PIX_W   (UVK5_LCD_WIDTH  * LCD_SCALE)
#define LCD_PIX_H   (UVK5_LCD_HEIGHT * LCD_SCALE)
#define BTN_W        60
#define BTN_H        30
#define BTN_GAP      4
#define PANEL_COLS   5
#define PANEL_ROWS   4
#define WINDOW_W     (LCD_PIX_W + PANEL_COLS * (BTN_W + BTN_GAP) + 40)
#define WINDOW_H     (LCD_PIX_H + 80)

static const char *btn_labels[PANEL_ROWS][PANEL_COLS] = {
    { "SIDE1", "MENU",  "UP",   "DOWN",  "EXIT" },
    { "SIDE2", "1",     "2",    "3",     "F"    },
    { "",      "4",     "5",    "6",     "STAR" },
    { "",      "7",     "8",    "9",     "0"    },
};

static HWND hwnd_main;
static HWND hwnd_btn[PANEL_ROWS][PANEL_COLS];
static HFONT hfont_btn;

static void render_lcd(HDC hdc, int ox, int oy)
{
    unsigned char raw[1024];
    if (!uvk5_get_framebuffer(raw, sizeof(raw)))
        return;

    unsigned int pixels[UVK5_LCD_WIDTH * UVK5_LCD_HEIGHT];
    decode_lcd(raw, pixels);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = UVK5_LCD_WIDTH;
    bmi.bmiHeader.biHeight      = -UVK5_LCD_HEIGHT; /* top-down */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchDIBits(hdc, ox, oy, LCD_PIX_W, LCD_PIX_H,
                  0, 0, UVK5_LCD_WIDTH, UVK5_LCD_HEIGHT,
                  pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

static void create_buttons(HWND parent, HINSTANCE hinst)
{
    hfont_btn = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    int x0 = LCD_PIX_W + 20;
    int y0 = 20;

    for (int r = 0; r < PANEL_ROWS; r++) {
        for (int c = 0; c < PANEL_COLS; c++) {
            if (!btn_labels[r][c][0]) continue;
            int x = x0 + c * (BTN_W + BTN_GAP);
            int y = y0 + r * (BTN_H + BTN_GAP);
            hwnd_btn[r][c] = CreateWindowA(
                "BUTTON", btn_labels[r][c],
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x, y, BTN_W, BTN_H,
                parent, (HMENU)(INT_PTR)(r * PANEL_COLS + c),
                hinst, NULL);
            SendMessage(hwnd_btn[r][c], WM_SETFONT, (WPARAM)hfont_btn, TRUE);
        }
    }
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        /* LCD area: 20, 20 */
        render_lcd(hdc, 20, 20);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1) {
            if (uvk5_is_lcd_dirty()) {
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } else if (wp == 2) {
            uvk5_key_release();
            KillTimer(hwnd, 2);
        }
        return 0;
    case WM_KEYDOWN:
        handle_keydown((int)wp, (lp >> 30) & 1);
        return 0;
    case WM_KEYUP:
        handle_keyup((int)wp);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        int r = id / PANEL_COLS;
        int c = id % PANEL_COLS;
        if (r >= 0 && r < PANEL_ROWS && c >= 0 && c < PANEL_COLS) {
            const char *name = btn_labels[r][c];
            if (name[0]) {
                uvk5_key_press(name);
                /* Auto-release after 200 ms */
                SetTimer(hwnd, 2, 200, NULL);
            }
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ----- Build command line for QEMU from executable path + defaults ----- */

static void build_qemu_argv(const char *exe_dir, char **argv_out, int *argc_out)
{
    char elf_path[MAX_PATH] = {0};
    char flash_path[MAX_PATH] = {0};

    /* Look for firmware.elf in the same directory as the exe. */
    snprintf(elf_path, sizeof(elf_path), "%s\\firmware.elf", exe_dir);
    snprintf(flash_path, sizeof(flash_path), "%s\\flash.img", exe_dir);

    /* Static argv -- QEMU parses these. */
    static char arg0[] = "qemu-system-arm";
    static char arg_m[] = "-M";
    static char arg_mval[] = "uv-k5-v3";       /* overwritten below */
    static char arg_nographic[] = "-nographic";
    static char arg_mon[] = "-monitor";
    static char arg_monval[] = "none";
    static char arg_qmp[] = "-qmp";
    static char arg_qmpval[] = "tcp:127.0.0.1:4444,server=on,wait=off";
    static char arg_gdb[] = "-gdb";
    static char arg_gdbval[] = "tcp::1234";
    static char arg_kernel[] = "-kernel";

    /* Build flash-image=... option. */
    static char arg_machine[] = "-M";
    char machine_opt[MAX_PATH + 32];
    snprintf(machine_opt, sizeof(machine_opt),
             "uv-k5-v3,flash-image=%s", flash_path);

    argv_out[0]  = arg0;
    argv_out[1]  = arg_machine;
    argv_out[2]  = machine_opt;
    argv_out[3]  = arg_nographic;
    argv_out[4]  = arg_mon;
    argv_out[5]  = arg_monval;
    argv_out[6]  = arg_qmp;
    argv_out[7]  = arg_qmpval;
    argv_out[8]  = arg_gdb;
    argv_out[9]  = arg_gdbval;
    argv_out[10] = arg_kernel;
    argv_out[11] = elf_path;
    *argc_out = 12;
}

/* ----- QEMU main-loop thread ----- */

static DWORD WINAPI qemu_thread_proc(LPVOID param)
{
    (void)param;
    /* qemu_init() was called without the BQL; acquire it before the main loop. */
    bql_lock();
    int status = qemu_main_loop();
    qemu_cleanup(status);
    bql_unlock();
    return 0;
}

/* ----- Entry point: replaces QEMU's main() ----- */

int uvk5_win32_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Determine directory of the executable. */
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    char *slash = strrchr(exe_path, '\\');
    if (slash) *slash = '\0';
    char *exe_dir = exe_path;

    /* Build QEMU arguments. */
    char *qemu_argv[16];
    int   qemu_argc;
    build_qemu_argv(exe_dir, qemu_argv, &qemu_argc);

    /* Initialize QEMU (creates machines, devices, starts CPU).
     * This must run without the BQL held. */
    qemu_init(qemu_argc, qemu_argv);

    /* Create the GUI window. */
    HINSTANCE hinst = GetModuleHandleA(NULL);
    WNDCLASSEXA wc = {
        .cbSize        = sizeof(wc),
        .lpfnWndProc   = wndproc,
        .hInstance     = hinst,
        .hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1),
        .lpszClassName = "UVK5Emu",
        .hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW),
    };
    RegisterClassExA(&wc);

    hwnd_main = CreateWindowExA(
        0, "UVK5Emu", "UV-K5 V3 Emulator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_W, WINDOW_H,
        NULL, NULL, hinst, NULL);

    create_buttons(hwnd_main, hinst);
    ShowWindow(hwnd_main, SW_SHOW);
    UpdateWindow(hwnd_main);

    /* 30 fps refresh timer. */
    SetTimer(hwnd_main, 1, 33, NULL);

    /* Start QEMU main loop on a background thread. */
    CreateThread(NULL, 0, qemu_thread_proc, NULL, 0, NULL);

    /* GUI event loop on the main thread (Windows requirement). */
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
