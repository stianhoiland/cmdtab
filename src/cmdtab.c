#include "cmdtab.h"

typedef struct title title_t;
typedef struct path path_t;
typedef struct linked_window linked_window_t;
typedef struct windows windows_t;
typedef struct keyboard keyboard_t;
typedef struct config config_t;
typedef struct selection selection_t;
typedef struct gdi gdi_t;

struct title {
    int length;
    wchar_t text[(MAX_PATH*2)+1];
    bool ok;
};

struct path {
    unsigned long length; // QueryFullProcessImageNameW() wants 'unsigned long'
    wchar_t text[(MAX_PATH*1)+1];
    bool ok;
};

struct linked_window {
    path_t exe_name;
    title_t title;
    path_t app_name;
    void *hico;
    void *hwnd;
    linked_window_t *next_sub_window;
    linked_window_t *prev_sub_window;
    linked_window_t *next_top_window;
    linked_window_t *prev_top_window;
    path_t exe_path;
};

//
// Structs for app state
//

struct windows {
    linked_window_t array[256]; // How many (filtered!) windows do you really need?!
    int count;
};

struct keyboard {
    char keys[32]; // 256 bits
};

struct config {
    unsigned long mod1;
    unsigned long key1;
    unsigned long mod2;
    unsigned long key2;
    bool switch_apps;
    bool switch_windows;
    bool wrap_bump;
    //bool fast_switching; // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
    bool ignore_minimized; // TODO? Atm, 'ignore_minimized' affects both Alt+` and Alt+Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.) TODO Should this be called 'restore_minimized'?
    unsigned show_delay;
    //wchar_t blacklist[32][(MAX_PATH*1)+1];
};

struct selection {
    linked_window_t *window; // Rename 'current'?
    linked_window_t *restore; // Rename 'initial'?
    bool active;
};

struct gdi {
    void *hdc;
    void *hbm;
    int width;
    int height;
    RECT rect;
    //HBRUSH bg;
};

//
// Global Windows API handles
//

static void *hMutexHandle;
static void *hMainInstance;
static void *hMainWindow;
static void *hKeyboardHook;

//
// Global variables for app state. Yes we could throw these into a 'struct app', but that encourages passing around too much state/context
//

static windows_t windows; // Filtered windows, as returned by EnumWindows(), filtered by filter() and added by add()

static keyboard_t keyboard; // Have to manually track key repeat for the low-level keyboard hook

static config_t config;

static selection_t selection;

static gdi_t gdi; // Off-screen bitmap used for double-buffering

//
// Uncomfortable forward declarations
//

static linked_window_t *first(linked_window_t *window, bool top_window); // Because I wanna put gdi_redraw() before first()

static void toggle_now(void *hwnd, unsigned message, uintptr_t id, unsigned long time); // Resolving circular dependency between toggle() and toggle_now()

//
// Here we go!
//

static void defaults(config_t *config) {
    //
    // Note about key2, used for switching sub windows
    // 
    // The default key2 is scan code 0x29 (hex) / 41 (decimal)
    // This is the key that is physically below Esc, left of 1 and above Tab
    //
    *config = (config_t){
        .mod1 = VK_LMENU,
        .key1 = VK_TAB,
        .mod2 = VK_LMENU,
        .key2 = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX),
        .switch_apps = true,
        .switch_windows = true,
        .wrap_bump = true,
        //.fast_switching = false, // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
        .ignore_minimized = false, // NOTE Currently only observed for subwindow switching and BUG! Move ignore_minimized handling from show() -> selectnext()
        .show_delay = 150,
        /*
        .blacklist = { // TODO blacklist isn't implemented yet
            L"TextInputHost.exe",
            L"SystemSettings.exe",
            L"ApplicationFrameHost.exe", // BUG Oof, this one is too heavy-handed. This excludes Calculator, for example. Have to find a better way to deal with UWP windows
        }
        */
    };
}

static bool once(void) { // aka. init()?
    static bool first = true;
    if (first) {
        first = !first;
        return true;
    } else {
        return false;
    }
}

// Low level debug printing w/ the following features: VARARG, wide char, formatted, safe
static void print(wchar_t *fmt, ...) {
    // TODO #ifdef this away for release builds
    va_list argp;
    va_start(argp, fmt);
    wchar_t wcs[4096];
    vswprintf_s(wcs, 4096, fmt, argp);
    va_end(argp);
    OutputDebugStringW(wcs);
}


static title_t title(void *hwnd) {
    title_t title = {
        .length = sizeof ((title_t *)0)->text / sizeof ((title_t *)0)->text[0], // BUG / TODO Wait, does size need -1 here?
        .text = {0},
        .ok = {0}
    };
    title.ok = GetWindowTextW(hwnd, title.text, title.length) > 0; // Length param: "The maximum number of characters to copy to the buffer, including the null character." (docs)
    title.length = GetWindowTextLengthW(hwnd); // Size [in characters!]
    return title;
}

static path_t path(void *hwnd) {
    path_t path = {
        .length = sizeof ((path_t *)0)->text / sizeof ((path_t *)0)->text[0], // BUG / TODO Wait, does size need -1 here? NOTE Used as buffer size (in) [in characters! (docs)] and path length (out) by QueryFullProcessImageNameW()
        .text = {0},
        .ok = {0}
    };
    //
    // How to get the file path of the executable for the process that spawned the window:
    // 
    // 1. GetWindowThreadProcessId() to get process id for window
    // 2. OpenProcess() for access to remote process memory
    // 3. QueryFullProcessImageNameW() [Windows Vista] to get full "executable image" name (i.e. exe path) for process
    //
    unsigned long pid;
    GetWindowThreadProcessId(hwnd, &pid);
    void *handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid); // NOTE See GetProcessHandleFromHwnd()
    if (!handle) {
        print(L"ERROR couldn't open process for window: %s %p\n", title(hwnd).text, hwnd);
        return path;
    } else {
        bool success = QueryFullProcessImageNameW(handle, 0, path.text, &path.length); // NOTE Can use GetLongPathName() if getting shortened paths
        if (!success) {
            print(L"ERROR couldn't get exe path for window: %s %p\n", title(hwnd).text, hwnd);
        }
        CloseHandle(handle);
        path.ok = success;
        return path;
    }
}

static bool patheq(path_t *path1, path_t *path2) {
    //
    // Compare paths from last to first char since unique paths nevertheless often have long identical prefixes
    // 
    // The normal way would be:
    // return (path1.length == path2.length && wcscmp(path1.path, path2.path) == 0);
    //
    if (path1->length != path2->length) {
        return false;
    }
    for (int i = path1->length - 1; i >= 0; i--) {
        if (path1->text[i] != path2->text[i]) {
            return false;
        }
    }
    return true;
}

static path_t filename(wchar_t *path, bool ext) {
    path_t name = {
        .length = sizeof((path_t *)0)->text / sizeof((path_t *)0)->text[0], // BUG / TODO Wait, does size need -1 here?
        .text = {0},
        .ok = {0}
    };
    errno_t cpy_error = wcscpy_s(name.text, name.length, PathFindFileNameW(path)); // BUG Not checking 'cpy_error'
    if (!ext) {
        PathRemoveExtensionW(name.text); // BUG Deprecated, use PathCchRemoveExtension()
    }
    name.length = wcslen(name.text);
    name.ok = (cpy_error == 0);
    return name;
}

static path_t appname(wchar_t *path) {
    // TODO Should maybe return 'title_t' so that people don't give filename() a 'path_t' returned by appname()
    //      We use 'path_t' as a return type just because it's convenient to not have to invent another struct
    //      We could use 'title_t', but we don't need 2x MAX_PATH (1x is enough)


    //
    // Get pretty application name
    //
    // Don't even ask me bro why it has to be like this
    // Thanks to 'Aric Stewart' for an ok-ish looking version of this
    //

    // Our return value:
    path_t name = {
        .length = sizeof((path_t *)0)->text / sizeof((path_t *)0)->text[0],
        .text = {0},
        .ok = {0}
    };

    unsigned length;
    unsigned long size, handle;
    void *version = NULL;
    wchar_t buffer[MAX_PATH];
    wchar_t *result;
    struct {
        unsigned short wLanguage;
        unsigned short wCodePage;
    } *translate;

    size = GetFileVersionInfoSizeW(path, &handle);
    if (!size) {
        goto done;
    }
    version = malloc(size);
    if (!version) {
        goto done;
    }
    if (!GetFileVersionInfoW(path, handle, size, version)) {
        goto done;
    }
    if (!VerQueryValueW(version, L"\\VarFileInfo\\Translation", (void *)&translate, &length)) {
        goto done;
    }
    wsprintfW(buffer, L"\\StringFileInfo\\%04x%04x\\FileDescription", translate[0].wLanguage, translate[0].wCodePage);
    if (!VerQueryValueW(version, buffer, (void *)&result, &length)) {
        goto done;
    }
    if (!length || !result) {
        goto done;
    }
    
    name.ok = wcscpy_s(name.text, name.length, result) == 0;
    name.length = length;

    done: {
        free(version);
    }

    return name;
}

static void *icon(wchar_t *path, void *hwnd) {
    //
    // HICON GetHighResolutionIcon(wchar_t *path, void *hwnd);
    // 
    // Requires the following:
    // 
    // #define COBJMACROS
    // #include "commoncontrols.h"
    // 
    // SHGetFileInfoW() docs:
    //   "You should call this function from a background thread. Failure to do so could cause the UI to stop responding." (docs)
    //   "If SHGetFileInfo returns an icon handle in the hIcon member of the SHFILEINFO structure pointed to by psfi, you are responsible for freeing it with DestroyIcon when you no longer need it." (docs)
    //   "You must initialize Component Object Model (COM) with CoInitialize or OleInitialize prior to calling SHGetFileInfo." (docs)
    //
    if (FAILED(CoInitialize(NULL))) { // In case we use COM
        return NULL;
    }
    SHFILEINFOW sfi = {0};
    if (!SHGetFileInfoW(path, 0, &sfi, sizeof sfi, SHGFI_SYSICONINDEX)) { // 2nd arg: -1, 0 or FILE_ATTRIBUTE_NORMAL
        debug(); // SHGetFileInfoW() has special return with SHGFI_SYSICONINDEX flag, which I think should never fail (docs unclear?)
        return NULL;
    }
    IImageList *piml = {0};
    if (FAILED(SHGetImageList(SHIL_JUMBO, &IID_IImageList, (void **)(&piml)))) {
        return NULL;
    }
    HICON hico = {0};
    int w, h;
    IImageList_GetIcon(piml, sfi.iIcon, ILD_TRANSPARENT, &hico); //ILD_IMAGE
    IImageList_GetIconSize(piml, &w, &h);
    IImageList_Release(piml);
    return hico;
    /*
    // Above is icon from module path
    // Below is icon from hwnd
    //HWND hwnd;
    //HICON hico;
    // Get the window icon
    if (hico = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0)) return hico;
    if (hico = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0)) return hico;
    // Alternative: Get from the window class (SM means "small")
    if (hico = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON)) return hico;
    if (hico = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM)) return hico;
    // Alternative: Use OS default icon
    if (hico = LoadIconW(NULL, IDI_WINLOGO)) return hico;
    if (hico = LoadIconW(NULL, IDI_APPLICATION)) return hico;
    // Alternative: Get the first icon from the main module (executable image of the process)
    if (hico = LoadIconW((HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE), MAKEINTRESOURCEW(0))) return hico; // BUG Doesn't work?
    */
}


static int width(windows_t *windows) {
    // TODO Duuude, let's not do it this way. Figure out something better
    int width = 64;
    int hor = 16;
    int count = 0;

    linked_window_t *window = &windows->array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
    while (window) {
        count++;
        if (window->next_top_window) {
            window = window->next_top_window;
        } else {
            break;
        }
    }

    return (width * count) + (hor * count) + (hor * 2);
}


static void gdi_init(gdi_t *gdi, void *hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    if (EqualRect(&gdi->rect, &rect)) {
        //print(L"EqualRect\n");
        return;
    } else {
        print(L"new hbm\n");
    }

    gdi->rect = rect;
    gdi->width = rect.right - rect.left;
    gdi->height = rect.bottom - rect.top;

    DeleteObject(gdi->hbm);
    DeleteDC(gdi->hdc);

    HDC hdc = GetDC(hwnd);
    gdi->hdc = CreateCompatibleDC(hdc);
    gdi->hbm = CreateCompatibleBitmap(hdc, gdi->width, gdi->height);
    SelectObject(gdi->hdc, gdi->hbm);
}

static void gdi_redraw(gdi_t *gdi, void *hwnd) {
#define HIGHLIGHT RGB(76, 194, 255)
#define BACKGROUND RGB(32, 32, 32) // dark mode?
#define ITEM_BG RGB(11, 11, 11)
#define TEXT_COLOR RGB(235, 235, 235)

#define OUTLINE 3
#define VERT_PAD 20
#define HORI_PAD 16
#define ICON_WIDTH 64
#define ROUNDED 10
    // TODO Create a 'style_t' type
    // TODO This is a mess atm. Working sorta fine, with some leaks I imagine, and looking rather terrible. FIX!

    gdi_init(gdi, hwnd);

    RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

    RECT rc = {0};
    //GetClientRect(WindowFromDC(gdi->hdc), &rc);
    GetClientRect(hwnd, &rc);

    const int ver = VERT_PAD; // Vertical padding (top)
    const int hor = HORI_PAD; // Horizontal padding
    const int width = ICON_WIDTH; // Icon width
    int i = 0;

    static HBRUSH icon_brush = {0};
    static HBRUSH bg_brush = {0};
    static HBRUSH selection_brush = {0};
    if (bg_brush == NULL) {
        //bg_brush = CreateSolidBrush(RGB(0, 0, 0));
        //bg_brush = (HBRUSH)(COLOR_BTNFACE + 1);
        //bg_brush = CreateSolidBrush(RGB(212, 208, 200));
        bg_brush = CreateSolidBrush(BACKGROUND);
        icon_brush = bg_brush;
        selection_brush = CreateSolidBrush(ITEM_BG);
    }

    FillRect(gdi->hdc, &rc, bg_brush);

    //rc.bottom = rc.bottom - (ver / 2);

    linked_window_t *window;

    i = 0;
    window = &windows.array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
    while (window) {

        // Draw selection rectangle
        linked_window_t *top1 = first(window, false);
        linked_window_t *top2 = first(selection.window, false);
        if (top1 == top2) { // Check if the windows are the same app

            //int center = (width * i) + (width / 2) + (hor * i);

            //rc.left = hor + center - (width / 2);// -width; // - width means make the rect wider to the left
            //rc.right = hor + center + (width / 2);// +width; // + width means make the rect wider to the right

            int x = (HORI_PAD + ICON_WIDTH) * i;

            RECT rc2 = (RECT){
                .left = x + 8,
                .top = rc.top + 8 + 4,
                .right = x + 64 + HORI_PAD + 8,
                .bottom = rc.top + 64 + HORI_PAD + 8 + 4
            };
            //rc2.left += 32;
            //rc2.right -= 32;
            //FillRect(gdi->hdc, &rc2, CreateSolidBrush(RGB(200, 200, 200)));
            //SelectObject(gdi->hdc, GetStockObject(GRAY_BRUSH));
            
            HPEN new_pen = CreatePen(PS_SOLID, OUTLINE, HIGHLIGHT);
            HPEN old_pen = (HPEN)SelectObject(gdi->hdc, new_pen);
            HBRUSH new_brush = CreateSolidBrush(ITEM_BG);
            HBRUSH old_brush = (HBRUSH)SelectObject(gdi->hdc, new_brush);
            RoundRect(gdi->hdc, rc2.left, rc2.top, rc2.right, rc2.bottom, ROUNDED, ROUNDED);
        }

        if (window->next_top_window) {
            window = window->next_top_window;
            i++;
        } else {
            break;
        }
    }

    rc.bottom = rc.bottom - 4; //(ver / 2);
    i = 0;
    window = &windows.array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
    while (window) {

        int center = (width * i) + (width / 2) + (hor * i);

        rc.left = hor + center - (width / 2) - width; // - width means make the rect wider to the left
        rc.right = hor + center + (width / 2) + width; // + width means make the rect wider to the right

        linked_window_t *top1 = first(window, false);
        linked_window_t *top2 = first(selection.window, false);

        if (top1 == top2) {
            icon_brush = selection_brush;
        } else {
            icon_brush = bg_brush;
        }

        // Draw icon
        void *hico = window->hico;//icon(window->exe_path.text, window->hwnd);
        DrawIconEx(gdi->hdc, hor + (width * i) + (hor * i), ver, hico, ICON_WIDTH, ICON_WIDTH, 0, icon_brush, DI_NORMAL);

        // Draw app name
        if (top1 == top2) { // Check if the windows are the same app
            wchar_t *title;
            if (window->app_name.ok) {
                title = window->app_name.text;
            } else {
                title = window->exe_name.text;
            }
            rc.bottom -= 3;
            //LOGFONT logfont;
            //GetObject(hFont, sizeof(LOGFONT), &logfont);

            SelectObject(gdi->hdc, GetStockObject(DEFAULT_GUI_FONT)); // TODO / BUG Leaks, I believe? Have to capture and later release return value when selecting object?
            //SelectObject(gdi->hdc, GetFont(L"Microsoft Sans Serif", 15));
            SetTextColor(gdi->hdc, TEXT_COLOR);
            //SetBkColor(gdi->hdc, RGB(200, 200, 200));
            SetBkColor(gdi->hdc, ITEM_BG);
            SetBkMode(gdi->hdc, TRANSPARENT);
            DrawTextW(gdi->hdc, title, -1, &rc, DT_CENTER | DT_SINGLELINE | DT_BOTTOM);
        }

        if (window->next_top_window) {
            window = window->next_top_window;
            i++;
        } else {
            break;
        }
    }
}

static void show(void *hwnd, bool restore) {
    if (hwnd == GetForegroundWindow()) { // TODO Should this really be here... Where should the responsibility lie of not trying to show the same window again?
        //debug();
        return;
    }
    // Restore from minimized if requested && neccessary
    if (restore && IsIconic(hwnd)) { // TODO Remove this comment? TODO Add config var to ignore minimized windows?? Nah...
        BOOL windowWasPreviouslyVisible = ShowWindow(hwnd, SW_RESTORE);
        /* dbg */ print(L"show (restore)\n");
    } else {
        /* dbg */ print(L"show\n");
    }

    // TODO / BUG / HACK! 
    // Oof, the rabbit hole...
    // There are rules for which processes can set the foreground window. See docs.
    // Sending an Alt key event allows us to circumvent limitations on SetForegroundWindow(). 
    // I believe (?!) that the real reason we need this hack as per the current implementation, is that we are technically
    // calling cycle() (which calls show(), which calls SetForegroundWindow()) *from the low level keyboard hook* and not
    // from our window thread (?), and the low level keyboard hook is not considered a foreground process.
    // I haven't tested this yet, but I believe the more correct implementation would be to SendMessageW() from the LLKBDH
    // to our window and call cycle() et al. from our window's WndProc. I think this more correct implementation is rather
    // trivial to set up, but I can't be bothered right now. So, Alt key hack it is.
    keybd_event(VK_MENU, 0x38, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

    SetForegroundWindow(hwnd);
}
/*
static void peek(void *hwnd) { //
    //
    // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
    //
    
    //bool windowGotShown = !ShowWindow(hwnd, SW_SHOWNA);
    bool success = SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

}
*/
static void toggle(bool show, unsigned delay) {
    // Toggle hMainWindow visibility (our switcher window/UI)
    // 'delay' allows quick switching without flickering UI
    // (We don't support delayed hiding (why would we?))

    if (show && IsWindowVisible(hMainWindow)) {
        return;
    }

    // Delay
    if (show && delay > 0) {
        // Will call toggle(true, 0) after 'delay'
        uintptr_t id = SetTimer(hMainWindow, 1, delay, toggle_now);
        return;
    } else {
        // We're showing or hiding *now*, so kill any pending show-timers
        KillTimer(hMainWindow, 1);
    }

    // Toggle visibility & set position of window to center
    if (show) {
        int w = 320; w = width(&windows); // See width() :(
        int h = 155;
        int x = GetSystemMetrics(SM_CXSCREEN) / 2 - (w / 2);
        int y = GetSystemMetrics(SM_CYSCREEN) / 2 - (h / 2);
        SetWindowPos(hMainWindow, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
        //SetFocus(hMainWindow);
        //SetForegroundWindow(hMainWindow);
        //ShowWindow(hMainWindow, SW_SHOW);
    } else {
        ShowWindow(hMainWindow, SW_HIDE);
    }
}

static void toggle_now(void *hwnd, unsigned message, uintptr_t id, unsigned long time) {
    toggle(true, 0);
}


static bool filter(void *hwnd) {

    // Window must be visible
    if (IsWindowVisible(hwnd) == false) {
        return false;
    }

    // Window must be visible. It's not enough to check WS_VISIBLE. Must use IsWindowVisible(), which also checks full window hierarchy (I think that's what IsWindowVisible() does?)
    //if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_VISIBLE) {
        //return false;
    //}


    // Ignore windows without title text
    //if (GetWindowTextLengthW(hwnd) == 0) {
        //return false;
    //}

    // Ignore desktop window
    //if (GetShellWindow() == hwnd) {
        //return false;
    //}

    // Ignore ourselves (we don't need to, because atm we only load windows and filter them while our window is not visible). Also, we're a WS_EX_TOOLWINDOW (see below)
    //if (hwnd == hMainWindow) { // TODO After nailing down the other filters, test if this is necessary anymore
        //return false;
    //}

    //if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) {
        //return false;
    //}


    if (config.ignore_minimized && IsIconic(hwnd)) {
        return false;
    }

    int cloaked = 0;
    int success = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked); // Windows 8
    if (success && cloaked) {
        return false;
    }


    // "A tool window [i.e. WS_EX_TOOLWINDOW] does not appear in the taskbar or in the dialog that appears when the user presses ALT+TAB." (docs)
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
        return false;
    }

    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
        return false;
    }

    if ((GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) && !(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW)) { // TODO We should actually ignore WS_CHILD if the window has WS_EX_APPWINDOW
        debug(); // This shouldn't happen, as we're using EnumWindows(), which doesn't enumerate child windows
        return false;
    }

    // Window should have no owner (i.e. be a "top-level window")
    HWND owner1 = GetWindow(hwnd, GW_OWNER);
    HWND owner2 = GetAncestor(hwnd, GA_ROOTOWNER);
    if (owner1 != NULL || owner2 != hwnd) {
        //debug(); // This shouldn't happen, as we're using EnumWindows(), which (mostly) enumerates top-level windows. Huh, it does happen! (When Windows' Alt-Tab is open??)
        return false;
    }
    
    WINDOWPLACEMENT placement = {0};
    /* bool */ success = GetWindowPlacement(hwnd, &placement);
    RECT rect = placement.rcNormalPosition;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int size = width + height;
    if (size < 100) { // window is less than 100 pixels
        return false;
    }
        
    /* Raymond Chen - Which windows appear in the Alt+Tab list?
    // Start at the root owner
    HWND hwndWalk = GetAncestor(hwnd, GA_ROOTOWNER);
    // See if we are the last active visible popup
    HWND hwndTry;
    while ((hwndTry = GetLastActivePopup(hwndWalk))!=hwndTry) {
        if (IsWindowVisible(hwndTry)) {
            break;
        }
        hwndWalk = hwndTry;
    }
    return (hwndWalk == hwnd);
    */

    return true;
}

static bool add(void *hwnd, void *context) {
    if (filter(hwnd)) {
        windows_t *windows = (windows_t *)context;

        path_t exe_path = path(hwnd);
        if (!exe_path.ok) {
            // IMPORTANT! Although there are comments elsewhere that 'exe_path'
            //            field is just for dbg'ing here we use it to avoid
            //            adding problematic windows/processes to the window
            //            list, and fail gracefully by simply excluding them
            //            from the switcher.
            print(L"whoops, bad exe path?! %p\n", hwnd);
            return true; // Return true to continue EnumWindows()
                         // which add() is a callback for
        }
        
        path_t exe_name = filename(exe_path.text, false);
        path_t app_name = appname(exe_path.text);
        title_t window_title = title(hwnd);
        void *hico = icon(exe_path.text, hwnd);

        //* dbg */ if (app_name.ok) {
        //* dbg */     print(L"%s (%i) %s\n", app_name.text, app_name.length, app_name.ok ? L"ok" : L"NOT OK");
        //* dbg */ } else {
        //* dbg */     print(L"%s (%i) %s\n", app_name.ok ? L"ok" : L"NOT OK", app_name.length, exe_name.text);
        //* dbg */ }

        /* To compound literal or not, that is the question...
        windows->array[windows->count++] = (linked_window_t){
            .exe_name = exe_name, // dbg. See note in 'struct linked_window' declaration
            .title = window_title, // dbg. See note in 'struct linked_window' declaration
            .app_name = app_name,
            .hico = hico,
            .hwnd = hwnd,
            .next_sub_window = NULL,
            .prev_sub_window = NULL,
            .next_top_window = NULL,
            .prev_top_window = NULL,
            .exe_path = exe_path // dbg. See note in 'struct linked_window' declaration
        };
        */
        linked_window_t *window = &windows->array[windows->count];
        windows->count++;
        window->exe_name = exe_name;
        window->title = window_title;
        window->app_name = app_name;
        window->hico = hico;
        window->hwnd = hwnd;
        window->next_sub_window = NULL;
        window->prev_sub_window = NULL;
        window->next_top_window = NULL;
        window->prev_top_window = NULL;
        window->exe_path = exe_path;
        

        //* dbg */ print(L"+ ");
        //* dbg */ print(L"%s \"%s\" - %s\n", exe_name.text, title.text, exe_path.text);
    } else {
        //* dbg */ print(L"- ");
        //* dbg */ print(L"%s \"%s\" - %s\n", exe_name.text, title.text, exe_path.text);
    }
    return true; // Return true to continue EnumWindows() (to which add() is a callback)
}

static void link(windows_t *windows) {
    // What should this function be called?
    // 'enumerate', 'fetch', 'populate', 'build', 'gather', 'collect', 'addall'

    // We're starting from scratch so go get y'all windows
    windows->count = 0;
    //* dbg */ print(L"EnumWindows (all windows) (+ means Alt-Tab window, - means not Alt-Tab window)\n");
    EnumWindows(add, windows); // (WNDENUMPROC)add, (LPARAM)windows);
    // Are there windows to link?
    if (windows->count < 2) {
        return;
    }

    //
    // A top window is the first window of its app in the windows list (the order of the window list is determined by EnumWindows())
    // A sub window is any window of the same app after its top window
    // A top window is also counted as its app's first sub window (this is only relevant for first())
    //
    // So after setting up links according to these rules, it looks like this: 
    //
    //   top_window ←→ top_window ←→ top_window ←→ top_window ←→ ...
    //        ↑             ↑             ↑
    //        ↓             ↓             ↓          
    //   sub_window    sub_window    sub_window      
    //        ↑
    //        ↓                                    
    //   sub_window                                
    // 
    for (int i = 1; i < windows->count; i++) {
        linked_window_t *window1 = &windows->array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
        linked_window_t *window2 = &windows->array[i];
        if (!window1->exe_path.ok || !window2->exe_path.ok) {
            print(L"ERROR whoops, bad exe paths?! %p %p\n", window1->hwnd, window2->hwnd);
            return; // BUG Use assert, i.e. crash()
        }
        // Determine if window2 is either
        //  a) of the same app as a prior top window (i.e. a sub window)
        //  b) the first window of its app in the window list (i.e. a top window)
        while (window1) {
            if (patheq(&window1->exe_path, &window2->exe_path)) {
                // TODO remove this dbg'ing
                /* dbg */ HWND owner1 = GetWindow(window1->hwnd, GW_OWNER);
                /* dbg */ HWND owner2 = GetAncestor(window1->hwnd, GA_ROOTOWNER);
                /* dbg */ if (owner1 != NULL && owner2 != NULL) {
                /* dbg */     //debug();
                /* dbg */ }
                while (window1->next_sub_window) { window1 = window1->next_sub_window; } // Get last subwindow of window1 (since window1 is a top window)
                window1->next_sub_window = window2;
                window2->prev_sub_window = window1;
                break;
            }
            if (window1->next_top_window == NULL) {
                break;
            }
            window1 = window1->next_top_window; // Check next top window
        }
        if (window2->prev_sub_window == NULL) { // window2 was not assigned a previous window of the same app, thus window2 is a top window
            window2->prev_top_window = window1;
            window1->next_top_window = window2;
        }
    }
}

static void wipe(windows_t *windows) { // Can't call function 'remove' :( 'wipe', 'erase', 'delete'
    for (int i = 0; i < windows->count; i++) {
        if (windows->array[i].hico) {
            DestroyIcon(windows->array[i].hico);
        }
    }
}


static void print_window(linked_window_t *window, bool extended) {
    // TODO Maybe get rid of this function? It just sits so ugly between all these nice functions: add, link, dump, load...
    if (window) {
        if (extended) {
            // Don't look in here, it's messy
            wchar_t *visible           = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_VISIBLE) ? L"visible" : L"";
            wchar_t *child             = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_CHILD) ? L", child" : L"";
            wchar_t *disabled          = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_DISABLED) ? L", disabled" : L"";
            wchar_t *minimized         = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_ICONIC) ? L", minimized" : L"";
            wchar_t *popup             = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_POPUP) ? L", popup" : L"";
            wchar_t *popup_window      = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_POPUPWINDOW) ? L", popupwindow" : L"";
            wchar_t *tiled             = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_TILED) ? L", tiled" : L"";
            wchar_t *tiled_window      = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_TILEDWINDOW) ? L", tiledwindow" : L"";
            wchar_t *overlapped        = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_OVERLAPPED) ? L", overlapped" : L"";
            wchar_t *overlapped_window = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW) ? L", overlappedwindow" : L""; 
            wchar_t *dialog            = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_DLGFRAME) ? L", dialog" : L"";
            wchar_t *ex_tool           = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ? L", ex_tool" : L"";
            wchar_t *ex_appwindow      = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) ? L", ex_appwindow" : L"";
            wchar_t *ex_modal          = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_DLGMODALFRAME) ? L", ex_modal" : L"";
            wchar_t *ex_layered        = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) ? L", ex_layered" : L"";
            wchar_t *ex_noactivate     = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) ? L", ex_noactivate" : L"";
            wchar_t *ex_palette        = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_PALETTEWINDOW) ? L", ex_palette" : L"";
            wchar_t *ex_overlapped_window = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_OVERLAPPEDWINDOW) ? L", ex_overlapped_window" : L"";
            print(L"(%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s) ", 
                  visible, child, disabled, minimized, popup, popup_window, tiled, tiled_window, overlapped, overlapped_window, dialog,
                  ex_tool, ex_appwindow, ex_modal, ex_layered, ex_noactivate, ex_palette, ex_overlapped_window);
        }
        wchar_t *ws_iconic = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_ICONIC) ? L"(minimized)" : L"";
        print(L"%s ", ws_iconic);
        print(L"%s \"%s\" - %s\n", window->exe_name.text, window->title.text, window->exe_path.text);
    } else {
        print(L"NULL window\n");
    }
}

static void dump(windows_t *windows, bool fancy) {
    double mem = (double)(sizeof *windows + sizeof keyboard + sizeof config + sizeof selection + sizeof gdi) / 1024; // TODO 'keyboard', 'config', 'selection', 'gdi' are referencing global vars
    //print(L"________________________________\n");
    print(L"%i Alt-Tab windows (%.0fkb mem):\n", windows->count, mem);
    if (!fancy) {
        // Just dump the array
        for (int i = 0; i < windows->count; i++) {
            linked_window_t *window = &windows->array[i];
            print(L"%i ", i);
            print_window(window, false);
        }
    } else {
        // Dump the linked list
        linked_window_t *top_window = &windows->array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
        linked_window_t *sub_window = top_window->next_sub_window;
        while (top_window) {
            print(L"+ ");
            print_window(top_window, false);
            while (sub_window) {
                print(L"\t- ");
                print_window(sub_window, false);
                sub_window = sub_window->next_sub_window;
            }
            if (top_window->next_top_window == NULL) {
                break;
            }
            top_window = top_window->next_top_window;
            sub_window = top_window->next_sub_window;
        }
    }
    print(L"\n");
}


static linked_window_t *find(windows_t *windows, void *hwnd) {
    for (int i = 0; i < windows->count; i++) {
        if (windows->array[i].hwnd == hwnd) {
            return &windows->array[i];
        }
    }
    return NULL;
}

static linked_window_t *first(linked_window_t *window, bool top_window) {
    if (top_window) {
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
        while (window->prev_top_window) { window = window->prev_top_window; } // 2. Go to first top widndow
    } else {
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window (which is also technically "first sub window")
    }
    return window; // TODO WTF How does this work in next() without return value!?
} 

static linked_window_t *last(linked_window_t *window, bool top_window) {
    if (top_window) {
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
        while (window->next_top_window) { window = window->next_top_window; } // 2. Go to last top window
    } else {
        while (window->next_sub_window) { window = window->next_sub_window; } // 1. Go to last sub window
    }
    return window; // TODO WTF How does this work in next() without return value!?
}

static linked_window_t *next(linked_window_t *window, bool top_window, bool reverse_direction, bool allow_wrap) {
    // TODO / BUG Handle NULL window, i.e. there are NO windows to switch to

    //if (window == NULL) {
        //debug();
        //return window; // TODO Maybe too defensive. Check call hierarchy and semantics for if this is ever a possillity
    //}

    if (top_window) {
        // Make sure 'window' is its app's top window
        // 'false' means get the first sub window (i.e. the top window of the app of 'window')
        // 'true' would have meant get the first top window (i.e. windows[0])
        window = first(window, false); 
    }

    linked_window_t *next_window = NULL;

    if (top_window) {
        next_window = reverse_direction ? window->prev_top_window : window->next_top_window;
    } else {
        next_window = reverse_direction ? window->prev_sub_window : window->next_sub_window;
    }
    
    if (next_window) {
        return next_window;
    }

    if (!allow_wrap) { // '!next_window' implied
        return window;
    }

    bool alone;

    if (top_window) {
        alone = !window->next_top_window && !window->prev_top_window; // All alone on the top :(
    } else {
        alone = !window->next_sub_window && !window->prev_sub_window; // Top looking for sub ;)
    }

    if (!alone) { // '!next_window && allow_wrap' implied
        /* dbg */ print(L"wrap%s\n", reverse_direction ? L" to last" : L" to first");
        return reverse_direction ? last(window, top_window) : first(window, top_window);
    }
    // IMPORTANT! Here we choose that if there is no "next" window, the "next"
    //            window shall nevertheless be the passed window, i.e. "switch
    //            to itself". This has important consequences for our API. We
    //            could instead have returned NULL here, with *its* consequences.
    return window;
}

static void select_null(selection_t *selection) {
    selection->window = NULL;
    selection->restore = NULL;
    selection->active = false;
}

static void select_foreground(selection_t *selection) {
    linked_window_t *foreground = find(&windows, GetForegroundWindow()); // BUG! What do when no foreground window find, because foreground window filtered out?? (see below)
    if (!foreground) {
        foreground = &windows.array[0]; // TODO / BUG I'm not sure this is ok
        //foreground = GetTopWindow(GetDesktopWindow()); // TODO / BUG Is this better??
    }
    selection->window = foreground;
    selection->restore = foreground;
    selection->active = (foreground != NULL);

    if (!foreground) {
        // dbg There are no windows (akshually, none of the filtered windows is the foreground window)
        /* dbg */ print(L"whoops, we can't find the foreground window");
    } else {
        /* dbg */ print(L"foreground window: ");
        /* dbg */ print_window(selection->window, false);
    }
}


static void cycle(selection_t *selection, bool cycle_apps, bool reverse_direction, bool allow_wrap, bool should_switch, bool show_gui) { // 'instant', 'immediate', 'fast', can't use 'switch' or 'show'
    // I wanted to call this function 'switch' :(
    
    if (!selection->active) {
        // Rebuild our whole window list
        wipe(&windows);
        link(&windows);
        /* dbg */ dump(&windows, false);
        //gdi_init(&gdi, hMainWindow); // TODO Not entirely decided yet how to time redrawing
        select_foreground(selection);
    }

    linked_window_t *old = selection->window;
    linked_window_t *new = next(selection->window, cycle_apps, reverse_direction, allow_wrap); // TODO / BUG: Handle NULL window, i.e. there are NO windows to switch to. Also: Where should the responsibility lie of not trying to show the same window again?

    if (new == old && !should_switch) { // TODO Detail why this condition is here. Something to do with wanting different behavior when Alt+`-ing vs. Alt+Tab-ing
        return;
    }

    // Select next window
    selection->window = new;

    /* dbg */ // sizeof: Damn that looks scary xD It's not tho--it's just getting and adding together the size of the 'text' fields of 'path_t' and 'title_t'
    /* dbg */ wchar_t app_title[sizeof ((path_t *)0)->text + sizeof ((title_t *)0)->text + 1 + 8]; // +8
    /* dbg */ errno_t cpy_error;
    /* dbg */ cpy_error = wcscpy_s(app_title, sizeof ((path_t *)0)->text, selection->window->exe_name.text);
    /* dbg */ cpy_error = wcscat_s(app_title, 32, L" - "); // Danger! Count your characters and size! We've allocated +8 for this, which is 4 wchars (3 for string, 1 for NUL)
    /* dbg */ cpy_error = wcscat_s(app_title, sizeof ((title_t *)0)->text, selection->window->title.text);
    /* dbg */ //SetWindowTextW(hMainWindow, app_title);

    /* dbg */ print(L"next %s%s: ", cycle_apps ? L"app" : L"subwindow", allow_wrap ? L"" : L" (repeat)");
    /* dbg */ print_window(selection->window, false);


    //if (config.fast_switching) {
        //peek(selection->window->hwnd); // TODO Doesn't work yet. See config.fast_switching and peek()
    //}


    // Alt+`
    if (should_switch) {
        show(selection->window->hwnd, true); // TODO? Atm, 'ignore_minimized' is handled in filter(), which means that 'ignore_minimized' affects both Alt+` and Alt+Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.)
    }
    // Alt+Tab
    if (show_gui) {
        toggle(true, once() ? 0 : config.show_delay); // TODO This doesn't check if gui is already visible. Where should the responsibility lie of not trying to show the same window again?
    }

    gdi_redraw(&gdi, hMainWindow);
}

static bool done(selection_t *selection, bool should_switch, bool should_restore, bool hide_gui) {
    if (should_switch && selection->active) {
        if (!should_restore) {
            // TODO When fast_switching is implemented, it should do something here
            show(selection->window->hwnd, true); // Fixed BUG This doesn't check if selection->window->hwnd is the same as the foreground window, so it potentially shows a already shown window. Maybe check for that in show()?
            /* dbg */ print(L"done\n");
        } else {
            show(selection->restore->hwnd, true); // Fixed BUG This doesn't check if selection->window->hwnd is the same as the foreground window, so it potentially shows a already shown window. Maybe check for that in show()?
            /* dbg */ print(L"done (esc)\n");
        }
    }

    if (hide_gui) {
        toggle(false, 0);
    }

    select_null(selection);

    return true;
}

// TODO Rewrite this comment and put it where it should be
// keyboard->keys
// 
// Key repeat must be tracked manually in low level keyboard hook
// Manually track modifier state and key repeat with a bit array
// vkCode: "The code must be a value in the range 1 to 254." (docs)
// 256 bits for complete tracking of currently held virtual keys (vkCodes, i.e. VK_* defines).
// vkCode technically fits in a 'char' (see above), but we'll use the same type as the struct
static bool set_key(keyboard_t *keyboard, unsigned long vkcode, bool down) {
    bool already_down = BITTEST(keyboard->keys, vkcode);
    bool repeat = already_down && down;
    if (!down) {
        BITCLEAR(keyboard->keys, vkcode);
    }
    if (down && !repeat) {
        BITSET(keyboard->keys, vkcode);
    }
    return repeat;
}

static bool get_key(keyboard_t *keyboard, unsigned long vkcode) {
    return BITTEST(keyboard->keys, vkcode);
}


static intptr_t LowLevelKeyboardProc(int nCode, intptr_t wParam, intptr_t lParam) {
    if (nCode < 0 || nCode != HC_ACTION) { // "If code is less than zero, the hook procedure must pass the message to blah blah blah read the docs"
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // Info about the keyboard for the low level hook
    KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;
    unsigned long virtual_key = kb->vkCode;
    bool key_down = !(kb->flags & LLKHF_UP);
    bool key_repeat = set_key(&keyboard, virtual_key, key_down); // Manually track key repeat using 'keyboard->keys' (see above)

    // Typically:
    //  mod1+key1 = Alt+Tab 
    //  mod2+key2 = Alt+`   (scancode 0x29)
    // TODO Remove mod2 ? Just have one modifier ?
    bool shift_held  = get_key(&keyboard, VK_LSHIFT); // NOTE This is the *left* shift key
    bool mod1_held   = get_key(&keyboard, config.mod1);
    bool mod2_held   = get_key(&keyboard, config.mod2);
    bool key1_down   = virtual_key == config.key1 && key_down;
    bool key2_down   = virtual_key == config.key2 && key_down;
    bool mod1_up     = virtual_key == config.mod1 && !key_down;
    bool mod2_up     = virtual_key == config.mod2 && !key_down;
    bool esc_down    = virtual_key == VK_ESCAPE && key_down;

    // Extended window management functionality
    // TODO Maybe separate up/left & down/right, and use left/right for apps &
    //      up/down for windows? Especially since I think we're eventually gonna
    //      graphically put a vertical window title list under the horizontal
    //      app icon list
    bool enter_down  = virtual_key == VK_RETURN && key_down;
    bool delete_down = virtual_key == VK_DELETE && key_down;
    bool prev_down   = (virtual_key == VK_UP || virtual_key == VK_LEFT) && key_down;
    bool next_down   = (virtual_key == VK_DOWN || virtual_key == VK_RIGHT) && key_down;
    bool ctrl_held   = get_key(&keyboard, VK_LCONTROL); // NOTE This is the *left* ctrl key // BUG! What if the users chooses their mod1 or mod2 to be ctrl?
    bool keyQ_down   = virtual_key ==  0x51 && key_down;
    bool keyW_down   = virtual_key ==  0x57 && key_down;
    bool keyF4_down  = virtual_key == VK_F4 && key_down;

    // NOTE By returning a non-zero value from the hook procedure, the message
    //      is consumed and does not get passed to the target window
    intptr_t pass_message = 0; // i.e. 'false'
    intptr_t consume_message = 1; // i.e. 'true'

    // TODO Support:
    // [x] Alt+Tab to cycle apps (configurable hotkey)
    // [x] Alt+` to cycle windows (configurable hotkey)
    // [x] Shift to reverse direction
    // [x] Esc to cancel
    // [x] Restore window on cancel
    // [x] Arrow keys to cycle apps
    // [x] Enter to switch app
    // [x] Wrap bump on key repeat
    // [x] Input sink so keys don't bleed to underlying apps
    // [x] Flicker-free fast switch
    // [ ] Q to quit application (i.e. close all windows, like macOS)
    // [ ] W to close window (like Windows and macOS)
    // [ ] M to minimize/restore window (what about (H)iding?)
    // [ ] Delete to close window (like Windows 10 and up)
    // [-] Alt-F4 (atm closes cmdtab.exe)
    // [ ] Mouse click app icon to switch
    // [ ] Cancel switcher on mouse click outside
    // [ ] Close app with mouse click on a red "X" on icon
    // [ ] Show number of subwindows for each app
    // [ ] Show numbers on each app and allow quick jumping by pressing that number
    // [-] BUGGY! A hotkey to disable/enable
    // [ ] Support dark/light mode
    // [ ] Ctrl+Alt+Tab for "persistent" GUI

    // BUG! Can't use Ctrl+Alt+Tab, because this is the "persistent alt tab" hotkey
    // Ctrl+Alt+Tab (has to be high up since the condition checking conflicts with Alt+Tab (cuz Alt+Tab doesn't check for !ctrl_held)
    // BUG! This is really buggy when passing between Windows' Alt-Tab and ours
    //      Hanging modifiers, missing modifiers...
    //      Passing from Windows' Alt-Tab to ours doesn't even work (oh wait, is that because Ctrl+Alt+Tab is the persistent alt tab hotkey?)
    if (key1_down && mod1_held && ctrl_held) {
        bool enabled = config.switch_apps = !config.switch_apps; // NOTE: Atm, we only support a hotkey for toggling app switching, not toggling window switching (cuz why would we?)
        if (!enabled) {
            done(&selection, false, false, true);
            return pass_message;
        } else {
            // pass-through to Alt+Tab below
        }
    }
    // Alt+Tab
    if (key1_down && mod1_held && config.switch_apps) {
        intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens

        /*
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = 0;
        input.ki.wScan = 0x38; // Alt key
        input.ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_SCANCODE; //(isDown ? 0 : KEYEVENTF_KEYUP) | KEYEVENTF_SCANCODE;
        input.ki.time = 0;
        input.ki.dwExtraInfo = 0;
        SendInput(1, &input, sizeof input);
        */

        cycle(&selection, true, shift_held, !config.wrap_bump || !key_repeat, false, true);
        return consume_message; // Since we "own" this hotkey system-wide, we consume this message unconditionally
    }
    // Alt+`
    if (key2_down && mod2_held && config.switch_windows) {
        intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        cycle(&selection, false, shift_held, !config.wrap_bump || !key_repeat, true, false);
        return consume_message; // Since we "own" this hotkey system-wide, we consume this message unconditionally
    }
    // Alt+arrows. People ask for this, so...
    if (selection.active && ((next_down || prev_down) && mod1_held)) { // selection.active: You can't *start* window switching with Alt+arrows, but you can navigate the switcher with arrows when switching is active
        intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        cycle(&selection, true, prev_down, !config.wrap_bump || !key_repeat, false, false);
        return consume_message; // Since our swicher is active, we "own" this hotkey, so we consume this message
    }
    // Alt keyup
    // BUG! Aha! Lemme tell yalittle secret: Our low level hook doesn't observe "Alt keydown" because Alt can be pressed down for
    //      so many purposes on system level. We're only interested in Alt+Tab, which means that we only really check for "Tab keydown",
    //      and then transparently check if Alt was already down when Tab was pressed.
    //      This means that we let all "Alt keydown"s pass through our hook, even the "Alt keydown" that the user pressed right before
    //      pressing Tab, which is sort of "ours", but we passed it through anyways (can't consume it after-the-fact).
    //      The result is that there is a hanging "Alt down" that got passed to the foreground app which needs an "Alt keyup", or
    //      the Alt key gets stuck. Thus, we always pass through "Alt keyup"s, even if we used it for some functionality.
    //      In short:
    //      1) Our app never hooks Alt keydowns
    //      2) Our app hooks Alt keyups, but always passes it through on pain of getting a stuck Alt key because of point 1
    if (selection.active && mod1_up) {
        done(&selection, true, false, true);
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message
    }
    if (selection.active && mod2_up) {
        done(&selection, true, false, false);
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message
    }
    // Alt+Enter
    if (selection.active && ((mod1_held || mod2_held) && enter_down)) {
        done(&selection, true, false, true);
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message
    }
    // Alt+Esc
    if (selection.active && ((mod1_held || mod2_held) && esc_down)) { // BUG There's a bug here if mod1 and mod2 aren't both the same key. In that case we'll still pass through the key event, even though we should own it since it's not Alt+Esc
        intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        result = done(&selection, true, true, true);
        return result; // We don't "own"/monopolize Alt+Esc. It's used by Windows for another, rather useful, window switching mechaninsm, and we preserve that
    }
    // Alt+F4 & Alt+Q
    if (selection.active && ((mod1_held || mod2_held) && (keyF4_down || keyQ_down))) {
        if (keyF4_down) {
            print(L"F4\n");
            SendMessageW(hMainWindow, WM_CLOSE, 0, 0);
        }
        if (keyQ_down) {
            print(L"Q\n");
        }
    }
    // Alt+W
    if (selection.active && ((mod1_held || mod2_held) && keyW_down)) {
        print(L"W\n");
    }
    // Alt+Delete
    if (selection.active && ((mod1_held || mod2_held) && delete_down)) {
        print(L"Delete\n");
    }
    // Sink. Consume all other keystrokes when we're activated. Maybe make this
    // a config option in case shit gets weird with the switcher eating
    // everything when activated
    if (selection.active && (mod1_held || mod2_held)) {
        /* dbg */ //print(L"sink vkcode:%llu%s%s\n", virtual_key, key_down ? L" down" : L" up", key_repeat ? L" (repeat)" : L"");
        return consume_message;
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static intptr_t WndProc(void *hwnd, unsigned message, intptr_t wParam, intptr_t lParam) {
//#define WM_USER_TRAYICON WM_USER + 1

    switch (message) {
        case WM_CREATE: {
            defaults(&config);
            return 0;
        }
        case WM_ERASEBKGND: {
            return 1;
        }
        case WM_PAINT: {
            // Double buffered drawing:
            // We draw off-screen in-memory to 'gdi.hdc' whenever changes occur,
            // and whenever we get the WM_PAINT message we just blit the
            // off-screen buffer to screen ("screen" means the 'hdc' returned by
            // BeginPaint() passed our 'hwnd')
            PAINTSTRUCT ps = {0};
            HDC hdc = BeginPaint(hwnd, &ps);
            BitBlt(hdc, 0, 0, gdi.width, gdi.height, gdi.hdc, 0, 0, SRCCOPY);
            //int scale = 1.0;
            //SetStretchBltMode(gdi.hdc, HALFTONE);
            //StretchBlt(hdc, 0, 0, gdi.width, gdi.height, gdi.hdc, 0, 0, gdi.width * scale, gdi.height * scale, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY: {
            //print(L"WM_DESTROY\n");
            PostQuitMessage(0);
            return 0;
        }
        default: {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }
    //return 0;
}

static bool is_single_instance() {
    hMutexHandle = CreateMutexW(NULL, true, L"CmdTabSingleton"); // com.stianhoiland.cmdtab.mutex
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return false;
    } else {
        return true;
    }
}

static void prompt_autorun() {
    
    // Assemble the filepath value for the autorun reg key

    wchar_t exe_path[MAX_PATH]; // path
    
    wchar_t target[MAX_PATH*2]; // path + args
    
    wchar_t *args = L"--silent";

    GetModuleFileNameW(NULL, exe_path, sizeof exe_path / sizeof *exe_path);

    StringCchPrintfW(target, sizeof target / sizeof *target, L"\"%s\" %s", exe_path, args); // "sprintf"

    // To create a reg key:
    // RegCreateKeyExW - it will open or create the key
    // RegSetValueExW
    // RegCloseKey

    // To delete a reg key:
    // RegOpenKeyExW
    // RegDeleteValueW 
    // RegCloseKey

    HKEY reg_key;
    wchar_t *reg_key_path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    wchar_t *reg_key_name = L"CmdTab";

    LSTATUS success;

    int result = MessageBoxW(NULL, L"Start cmdtab.exe automatically? (Relaunch cmdtab.exe to change your mind.)", L"cmdtab", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL);

    switch (result) {
        case IDYES: {
            print(L"Adding startup reg key\n");
            success = RegCreateKeyExW(HKEY_CURRENT_USER, reg_key_path, 0, NULL, 0, KEY_SET_VALUE, NULL, &reg_key, NULL); // ERROR_SUCCESS
            success = RegSetValueExW(reg_key, reg_key_name, 0, REG_SZ, target, sizeof target); // ERROR_SUCCESS
            success = RegCloseKey(reg_key);
            break;
        }
        case IDNO: {
            print(L"Removing startup reg key\n");
            success = RegOpenKeyExW(HKEY_CURRENT_USER, reg_key_path, 0, KEY_ALL_ACCESS, &reg_key); // ERROR_SUCCESS
            success = RegDeleteValueW(reg_key, reg_key_name); // ERROR_SUCCESS, ERROR_FILE_NOT_FOUND
            success = RegCloseKey(reg_key);
            break;
        }
    }
}

//
// Windows API main()
//

int APIENTRY wWinMain(void *hInstance, void *hPrevInstance, wchar_t *lpCmdLine, int nCmdShow) {

    OutputDebugStringW(L"Ẅęļçǫɱę ťỡ ûňîĉôɗě\n");

    // Quit if CmdTab is already running
    if (!is_single_instance()) {
        /* dbg */ print(L"ERROR cmdtab is already running.\n");
        MessageBoxW(NULL, L"cmdtab.exe is already running.", L"cmdtab", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
        ExitProcess(0);
    } else {
    }

    // Don't prompt about autorun if we got passed "--silent" (which the actual autorun call passes)
    if (wcscmp(lpCmdLine, L"--silent") != 0) {
        prompt_autorun();
    } else {
        /* dbg */ print(L"got --silent\n");
        /* dbg */ MessageBoxW(NULL, L"Started cmdtab.exe automatically. (This is a harmless debugging message and should have been disabled for release versions.)", L"cmdtab", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
    }


    // https://stackoverflow.com/a/70693198/659310
    //#include <dwmapi.h>
    BOOL USE_DARK_MODE = true; // See below: SET_IMMERSIVE_DARK_MODE_SUCCESS
    
    HBRUSH hbrBackground = USE_DARK_MODE ? (HBRUSH)(CreateSolidBrush(RGB(32, 32, 32))) : (HBRUSH)(COLOR_WINDOW + 1); // Can't seem to remove the initial light window background flicker...


    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof wcex;
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = (WNDPROC)WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon   = LoadIconW(hInstance, IDI_APPLICATION); // "cmdtab.ico"
    wcex.hIconSm = LoadIconW(hInstance, IDI_APPLICATION); // "small.ico"
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = hbrBackground;
    wcex.lpszMenuName  = NULL;
    wcex.lpszClassName = L"CMDTAB";

    unsigned short class = RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW, // Maybe include WS_EX_TOPMOST // "WS_EX_TOOLWINDOW" is excluded in ALT-TAB according to docs, yay!
        class, //wcex.lpszClassName,
        NULL, // No title bar text
        0,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    BOOL SET_IMMERSIVE_DARK_MODE_SUCCESS = SUCCEEDED(DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE, sizeof(USE_DARK_MODE)));

    //SendMessageW(hWnd, WM_PAINT, 0, 0); // Can't seem to remove the initial light window background flicker...

    // This removes the window title bar, window shadow and window shape rounding
    //SetWindowLongPtrW(hMainWindow, GWL_STYLE, GetWindowLongPtrW(hMainWindow, GWL_STYLE) & ~WS_CAPTION);

    hMainInstance = hInstance;
    hMainWindow = hWnd;
    hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, (HOOKPROC)LowLevelKeyboardProc, NULL, 0); // NULL | hInstance | GetModuleHandleW(NULL)

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        //TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(hKeyboardHook);

    ReleaseMutex(hMutexHandle);
    CloseHandle(hMutexHandle);

    return (int)msg.wParam;
}
