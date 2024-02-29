#include "cmdtab.h"

typedef struct title title_t;
typedef struct path path_t;
typedef struct linked_window linked_window_t;
typedef struct windows windows_t;
typedef struct selection selection_t;
typedef struct keyboard keyboard_t;
typedef struct gdi gdi_t;
typedef struct config config_t;

typedef unsigned long vkcode;

struct title {
    wchar_t text[(MAX_PATH*2)+1];
    int length;
    bool ok;
};

struct path {
    wchar_t text[(MAX_PATH*1)+1];
    unsigned long length; // QueryFullProcessImageNameW() wants 'unsigned long'
    bool ok;
};

struct linked_window {
    path_t exe_path;
    path_t exe_name;
    path_t app_name;
    title_t title;
    void *hico;
    void *hwnd;
    linked_window_t *next_sub_window;
    linked_window_t *prev_sub_window;
    linked_window_t *next_top_window;
    linked_window_t *prev_top_window;
};

//==============================================================================
// Structs for app state
//==============================================================================

struct windows {
    linked_window_t array[256]; // How many (filtered!) windows do you really need?!
    int count;
};

struct selection {
    linked_window_t *window; // Rename 'current'?
    linked_window_t *restore; // Rename 'initial'?
    bool active;
};

struct keyboard {
    char keys[32]; // 256 bits for tracking key repeat
};

struct gdi {
    void *hdc;
    void *hbm;
    int width;
    int height;
    RECT rect;
    //HBRUSH bg;
};

struct config {
    vkcode mod1;
    vkcode key1;
    vkcode mod2;
    vkcode key2;
    bool switch_apps;
    bool switch_windows;
    bool wrap_bump;
    //bool fast_switching; // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
    bool ignore_minimized; // TODO? Atm, 'ignore_minimized' affects both Alt+` and Alt+Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.) TODO Should this be called 'restore_minimized'?
    unsigned gui_delay;
    //wchar_t blacklist[32][(MAX_PATH*1)+1];
};

//==============================================================================
// Global Windows API handles
//==============================================================================

static void *hMutexHandle;
static void *hMainInstance;
static void *hMainWindow;
static void *hKeyboardHook;

//==============================================================================
// Global variables for app state
//==============================================================================

static windows_t windows; // Filtered windows, as returned by EnumWindows(), filtered by filter() and added by add()

static selection_t selection; // State for currently selected window

static keyboard_t keyboard; // Must manually track key repeat for the low-level keyboard hook

static gdi_t gdi; // Off-screen bitmap used for double-buffering

static config_t config;

//==============================================================================
// Uncomfortable forward declarations
//==============================================================================

static BOOL CALLBACK EnumProc(HWND, LPARAM); // Used as callback for EnumWindows()

static VOID CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD); // Resolve circular dependency between show_gui() and TimerProc()

//==============================================================================
// Utility functions
//==============================================================================

static void print(wchar_t *fmt, ...) {
    // Low level debug printing w/ the following features:
    // vararg, wide char, formatted, safe (i.e. vswprintf_s)
    // TODO #ifdef this away for release builds?
    wchar_t wcs[4096];
    va_list argp;
    va_start(argp, fmt);
    vswprintf_s(wcs, countof(wcs), fmt, argp); // BUG Not checking return value for error
    va_end(argp);
    OutputDebugStringW(wcs);
}

static bool once(void) {
    static bool first = true;
    if (first) {
        first = !first;
        return true;
    } else {
        return false;
    }
}

static void SetForegroundWindow_ALTHACK(void *hwnd) {
    // TODO / BUG / HACK!
    // Oof, the rabbit hole...
    // There are rules for which processes can set the foreground window.
    // See docs. Sending an Alt key event allows us to circumvent limitations on
    // SetForegroundWindow(). I believe (?!) that the real reason we need this
    // hack as per the current implementation, is that we are technically
    // calling cycle() (which calls show(), which calls SetForegroundWindow())
    // *from the low level keyboard hook* and not from our window thread (?),
    // and the low level keyboard hook is not considered a foreground process.
    // I haven't tested this yet, but I believe the more correct implementation
    // would be to SendMessageW() from the LLKBDH to our window and call cycle()
    // et al. from our window's WndProc. I think this more correct
    // implementation is rather trivial to set up, but I can't be bothered right
    // now. So, Alt key hack it is.
    keybd_event(VK_MENU, 0x38, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

    SetForegroundWindow(hwnd);
}

//==============================================================================
// Here we go!
//==============================================================================

static title_t title(void *hwnd) {
    title_t title = {
        .text = {0},
        .length = countof(((title_t*)0)->text), // BUG / TODO Wait, does size need -1 here or bellow?
        .ok = {0}
    };
    title.length = GetWindowTextLengthW(hwnd); // Size [in characters!]
    title.ok = GetWindowTextW(hwnd, title.text, title.length) > 0; // Length param: "The maximum number of characters to copy to the buffer, including the null character." (docs)
    return title;
}

static path_t path(void *hwnd) {
    path_t path = {
        .text = {0},
        .length = countof(((path_t *)0)->text), // BUG / TODO Wait, does size need -1 here or below? NOTE Used as buffer size (in) [in characters! (docs)] and path length (out) by QueryFullProcessImageNameW()
        .ok = {0}
    };
    // How to get the file path of the executable for the process that spawned the window:
    // 1. GetWindowThreadProcessId() to get process id for window
    // 2. OpenProcess() for access to remote process memory
    // 3. QueryFullProcessImageNameW() [Windows Vista] to get full "executable image" name (i.e. exe path) for process
    unsigned long pid;
    GetWindowThreadProcessId(hwnd, &pid);
    void *process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid); // NOTE See GetProcessHandleFromHwnd()
    if (!process) {
        print(L"ERROR couldn't open process for window: %s %p\n", title(hwnd).text, hwnd);
        return path;
    } else {
        bool success = QueryFullProcessImageNameW(process, 0, path.text, &path.length); // NOTE Can use GetLongPathName() if getting shortened paths
        if (!success) {
            print(L"ERROR couldn't get exe path for window: %s %p\n", title(hwnd).text, hwnd);
        }
        CloseHandle(process);
        path.ok = success;
        return path;
    }
}

static bool patheq(path_t *path1, path_t *path2) {
    // Compare paths from last to first char since unique paths nevertheless often have long identical prefixes
    // The normal way would be:
    //  return (path1.length == path2.length && wcscmp(path1.path, path2.path) == 0);
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
        .text = {0},
        .length = countof(((path_t *)0)->text), // BUG / TODO Wait, does size need -1 here?
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
    // TODO Should maybe return 'title_t' so that people don't give filename()
    //      a 'path_t' returned by appname(). We use 'path_t' as a return type
    //      just because it's convenient to not have to invent another struct.
    //      We could use 'title_t', but we don't need 2x MAX_PATH (1x is enough)


    // Get pretty application name
    //
    // Don't even ask me bro why it has to be like this
    // Thanks to 'Aric Stewart' for an ok-ish looking version of this that I
    // adapted a little

    // Our return value:
    path_t name = {
        .text = {0},
        .length = countof(((path_t *)0)->text),
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

    name.length = length;
    name.ok = wcscpy_s(name.text, name.length, result) == 0;

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
    //   "You should call this function from a background thread. Failure to do
    //    so could cause the UI to stop responding." (docs)
    //   "If SHGetFileInfo returns an icon handle in the hIcon member of the
    //    SHFILEINFO structure pointed to by psfi, you are responsible for
    //    freeing it with DestroyIcon when you no longer need it." (docs)
    //   "You must initialize Component Object Model (COM) with CoInitialize or
    //    OleInitialize prior to calling SHGetFileInfo." (docs)
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
        .gui_delay = 150,
        /*
        .blacklist = { // TODO blacklist isn't implemented yet
            L"TextInputHost.exe",
            L"SystemSettings.exe",
            L"ApplicationFrameHost.exe", // BUG Oof, this one is too heavy-handed. This excludes Calculator, for example. Have to find a better way to deal with UWP windows
        }
        */
    };
}

static int numapps(windows_t *windows) {
    // TODO Don't use numapps() and width(); figure out something more elegant
    linked_window_t *window = &windows->array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
    int num_apps = 0;
    while (window) {
        num_apps++;
        if (window->next_top_window) {
            window = window->next_top_window;
        } else {
            break;
        }
    }
    return num_apps;
}

static int width(windows_t *windows) {
    int icon_width = 64;
    int horz_pad = 16;
    int count = numapps(windows);

    return (icon_width * count) + (horz_pad * count) + (horz_pad * 2);
}


static void show_hwnd(void *hwnd, bool restore) {
    if (restore && IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
    }
    SetForegroundWindow_ALTHACK(hwnd);
}
/*
static void peek_hwnd(void *hwnd) {
    //
    // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
    //

    //bool windowGotShown = !ShowWindow(hwnd, SW_SHOWNA);
    bool success = SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

}
*/
static void size_gui(void) {
    int w = width(&windows); // See width() :(
    int h = 155;
    int x = GetSystemMetrics(SM_CXSCREEN) / 2 - (w / 2);
    int y = GetSystemMetrics(SM_CYSCREEN) / 2 - (h / 2);
    MoveWindow(hMainWindow, x, y, w, h, false);
}

static void show_gui(unsigned delay) {
    KillTimer(hMainWindow, 1);
    if (delay > 0) {
        SetTimer(hMainWindow, 1, delay, TimerProc);
        return;
    }
    ShowWindow(hMainWindow, SW_SHOW);
    SetForegroundWindow(hMainWindow);
}

static void hide_gui(void) {
    KillTimer(hMainWindow, 1);
    ShowWindow(hMainWindow, SW_HIDE); // Yes, call ShowWindow() to hide window...
}

static VOID TimerProc(HWND hWnd, UINT uMessage, UINT_PTR uEventId, DWORD dwTime) {
    show_gui(0);
}


static bool filter(void *hwnd) {

    if (config.ignore_minimized && IsIconic(hwnd)) {
        return false;
    }
    // Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
    if (IsWindowVisible(hwnd) == false) {
        return false;
    }
    int cloaked = 0;
    int success = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked); // Windows 8
    if (success && cloaked) {
        return false;
    }
    // "A tool window [i.e. WS_EX_TOOLWINDOW] does not appear in the taskbar or
    // in the dialog that appears when the user presses ALT+TAB." (docs)
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
        return false;
    }

    // Window must be visible. It's not enough to check WS_VISIBLE. Must use
    // IsWindowVisible(), which also checks full window hierarchy (I think
    // that's what IsWindowVisible() does?)
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

    // Ignore ourselves (we don't need to, because atm we only load windows and
    // filter them while our window is not visible). Also, we're a
    // WS_EX_TOOLWINDOW (see below)
    //if (hwnd == hMainWindow) { // TODO After nailing down the other filters, test if this is necessary anymore
        //return false;
    //}

    //if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) {
        //return false;
    //}



    //if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
        //return false;
    //}

    // Omg, this one gets rid of many things that shouldn't appear in Alt-Tab
    // EDIT Oh noes, Calculator disappears!
    /*
    if (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUP) {
        //return false;
    }
    */
    /*
    if ((GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) && !(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW)) { // TODO We should actually ignore WS_CHILD if the window has WS_EX_APPWINDOW
        debug(); // This shouldn't happen, as we're using EnumWindows(), which doesn't enumerate child windows
        return false;
    }
    */
    // Window should have no owner (i.e. be a "top-level window")
    /*
    HWND owner1 = GetWindow(hwnd, GW_OWNER);
    HWND owner2 = GetAncestor(hwnd, GA_ROOTOWNER);
    if (owner1 != NULL || owner2 != hwnd) {
        //debug(); // This shouldn't happen, as we're using EnumWindows(), which
                   // (mostly) enumerates top-level windows.
                   // Huh, it does happen! (When Windows' Alt-Tab is open??)
        return false;
    }
    */

    /*
    WINDOWPLACEMENT placement = {0};
    bool success = GetWindowPlacement(hwnd, &placement);
    RECT rect = placement.rcNormalPosition;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int size = width + height;
    if (size < 100) { // window is less than 100 pixels
        return false;
    }
    */

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


static void add(windows_t* windows, void *hwnd) {
    path_t exe_path = path(hwnd);

    if (!exe_path.ok) {
        // IMPORTANT! Although there are comments elsewhere that 'exe_path'
        //            field is just for dbg'ing, here we use it to avoid
        //            adding problematic windows/processes to the window
        //            list, and fail gracefully by simply excluding them
        //            from the switcher.
        print(L"whoops, bad exe path?! %p\n", hwnd);
        return;
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

    linked_window_t *window = &windows->array[windows->count];
    windows->count++;
    window->exe_path = exe_path;
    window->exe_name = exe_name;
    window->app_name = app_name;
    window->title = window_title;
    window->hico = hico;
    window->hwnd = hwnd;
    window->next_sub_window = NULL;
    window->prev_sub_window = NULL;
    window->next_top_window = NULL;
    window->prev_top_window = NULL;
}

static void link(windows_t *windows) {
    //
    // A top window is the first window of its app in the windows list (the
    //  order of the window list is determined by EnumWindows())
    // A sub window is any window of the same app after its top window
    // A top window is also counted as its app's first sub window (this is only
    //  relevant for first())
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
    if (windows->count < 2) {
        return;
    }
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
                // a)
                while (window1->next_sub_window) { window1 = window1->next_sub_window; } // Get last subwindow of window1 (since window1 is a top window)
                window1->next_sub_window = window2;
                window2->prev_sub_window = window1;
                break;
            }
            if (window1->next_top_window) {
                window1 = window1->next_top_window; // Check next top window
            } else {
                break;
            }
        }
        if (window2->prev_sub_window == NULL) {
            // b) window2 was not assigned a previous window of the same app, thus window2 is a top window
            window2->prev_top_window = window1;
            window1->next_top_window = window2;
        }
    }
}

static void wipe(windows_t *windows) {
    for (int i = 0; i < windows->count; i++) {
        if (windows->array[i].hico) {
            DestroyIcon(windows->array[i].hico);
        }
    }
    windows->count = 0;
}

static void rebuild(windows_t *windows) {
    // Rebuild our whole window list
    wipe(windows);
    EnumWindows(EnumProc, 0); // EnumProc() calls filter() and add()
    link(windows);
}

static BOOL EnumProc(HWND hWnd, LPARAM lParam) {
    if (filter(hWnd)) {
        add(&windows, hWnd);
        //* dbg */ print(L"+ ");
        //* dbg */ print(L"%s \"%s\" - %s\n", exe_name.text, title.text, exe_path.text);
    } else {
        //* dbg */ print(L"- ");
        //* dbg */ print(L"%s \"%s\" - %s\n", exe_name.text, title.text, exe_path.text);
    }
    return true; // Return true to continue EnumWindows()
}


static void print_window(linked_window_t *window, bool details) {
    if (window) {
        if (details) {
            wchar_t *visible             = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_VISIBLE) ? L"visible" : L"";
            wchar_t *child               = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_CHILD) ? L", child" : L"";
            wchar_t *disabled            = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_DISABLED) ? L", disabled" : L"";
            wchar_t *popup               = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_POPUP) ? L", popup" : L"";
            wchar_t *popupwindow         = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_POPUPWINDOW) ? L", popupwindow" : L"";
            wchar_t *tiled               = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_TILED) ? L", tiled" : L"";
            wchar_t *tiledwindow         = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_TILEDWINDOW) ? L", tiledwindow" : L"";
            wchar_t *overlapped          = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_OVERLAPPED) ? L", overlapped" : L"";
            wchar_t *overlappedwindow    = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW) ? L", overlappedwindow" : L"";
            wchar_t *dlgframe            = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_DLGFRAME) ? L", dlgframe" : L"";
            wchar_t *ex_toolwindow       = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ? L", ex_toolwindow" : L"";
            wchar_t *ex_appwindow        = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) ? L", ex_appwindow" : L"";
            wchar_t *ex_dlgmodalframe    = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_DLGMODALFRAME) ? L", ex_dlgmodalframe" : L"";
            wchar_t *ex_layered          = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) ? L", ex_layered" : L"";
            wchar_t *ex_noactivate       = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) ? L", ex_noactivate" : L"";
            wchar_t *ex_palettewindow    = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_PALETTEWINDOW) ? L", ex_palettewindow" : L"";
            wchar_t *ex_overlappedwindow = (GetWindowLongPtrW(window->hwnd, GWL_EXSTYLE) & WS_EX_OVERLAPPEDWINDOW) ? L", ex_overlappedwindow" : L"";
            print(L"(%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s) ",
                visible, child, disabled, popup, popupwindow, tiled, tiledwindow, overlapped, overlappedwindow, dlgframe,
                ex_toolwindow, ex_appwindow, ex_dlgmodalframe, ex_layered, ex_noactivate, ex_palettewindow, ex_overlappedwindow);
        }
        wchar_t *ws_iconic = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_ICONIC) ? L"(minimized)" : L"";
        print(L"%s %s \"%s\" - %s\n", ws_iconic, window->exe_name.text, window->title.text, window->exe_path.text);
    } else {
        print(L"NULL window\n");
    }
}

static void print_all(windows_t *windows, bool fancy) {
    double mem = (double)(
        sizeof *windows +
        sizeof selection +
        sizeof keyboard +
        sizeof gdi +
        sizeof config) / 1024; // TODO Referencing global vars
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
    print(L"________________________________\n");
}


static linked_window_t *find_hwnd(windows_t *windows, void *hwnd) {
    for (int i = 0; i < windows->count; i++) {
        if (windows->array[i].hwnd == hwnd) {
            return &windows->array[i];
        }
    }
    return NULL;
}

static linked_window_t *find_first(linked_window_t *window, bool top_level) {
    if (top_level) {
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
        while (window->prev_top_window) { window = window->prev_top_window; } // 2. Go to first top widndow
    } else {
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window (which is also technically "first sub window")
    }
    return window; // TODO WTF How does this work in next() without return value!?
}

static linked_window_t *find_last(linked_window_t *window, bool top_level) {
    if (top_level) {
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
        while (window->next_top_window) { window = window->next_top_window; } // 2. Go to last top window
    } else {
        while (window->next_sub_window) { window = window->next_sub_window; } // 1. Go to last sub window
    }
    return window; // TODO WTF How does this work in next() without return value!?
}

static linked_window_t *find_next(linked_window_t *window, bool top_level, bool reverse_direction, bool allow_wrap) {
    // TODO / BUG Handle NULL window, i.e. there are NO windows to switch to

    //if (window == NULL) {
        //print(L"window is NULL");
    //}

    if (top_level) {
        // Ensure that 'window' is its app's top window
        // 'false' means get the first sub window (i.e. the top window of the app of 'window')
        // 'true' would have meant get the first top window (i.e. windows[0])
        window = find_first(window, false);
    }

    linked_window_t *next_window = NULL;

    if (top_level) {
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

    if (top_level) {
        alone = !window->next_top_window && !window->prev_top_window; // All alone on the top :(
    } else {
        alone = !window->next_sub_window && !window->prev_sub_window; // Top looking for sub ;)
    }

    if (!alone) { // '!next_window && allow_wrap' implied
        /* dbg */ print(L"wrap%s\n", reverse_direction ? L" to last" : L" to first");
        return reverse_direction ? find_last(window, top_level) : find_first(window, top_level);
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
    linked_window_t *foreground = find_hwnd(&windows, GetForegroundWindow());
    if (foreground == NULL) {
        // TODO / BUG  What do when no foreground window find, because foreground window filtered OUT??
        // TODO / BUG  I'm not sure this is ok:
        foreground = &windows.array[0]; // BUG Can stil be empty
        // TODO / BUG Is this better??
        //foreground = GetTopWindow(GetDesktopWindow());
    }
    selection->window = foreground;
    selection->restore = foreground;
    selection->active = (foreground != NULL);

    if (foreground == NULL) {
        // dbg There are no windows (akshually, none of the filtered windows is the foreground window)
        /* dbg */ print(L"whoops, we can't find the foreground window");
    } else {
        /* dbg */ print(L"foreground window: ");
        /* dbg */ print_window(selection->window, false);
    }
}

static void select_next(selection_t *selection, bool top_level, bool reverse_direction, bool allow_wrap) {
    linked_window_t *new = find_next(selection->window, top_level, reverse_direction, allow_wrap);
    // Select next window
    selection->window = new;

    ///* dbg */ // sizeof: Damn that looks scary xD It's not tho--it's just getting and adding together the size of the 'text' fields of 'path_t' and 'title_t'
    ///* dbg */ wchar_t app_title[sizeof ((path_t *)0)->text + sizeof ((title_t *)0)->text + 1 + 8]; // +8
    ///* dbg */ errno_t cpy_error;
    ///* dbg */ cpy_error = wcscpy_s(app_title, sizeof ((path_t *)0)->text, selection->window->exe_name.text);
    ///* dbg */ cpy_error = wcscat_s(app_title, 32, L" - "); // Danger! Count your characters and size! We've allocated +8 for this, which is 4 wchars (3 for string, 1 for NUL)
    ///* dbg */ cpy_error = wcscat_s(app_title, sizeof ((title_t *)0)->text, selection->window->title.text);
    ///* dbg */ SetWindowTextW(hMainWindow, app_title);

    /* dbg */ print(L"selected %s%s: ", top_level ? L"app" : L"subwindow", allow_wrap ? L"" : L" (repeat)");
    /* dbg */ print_window(selection->window, false);


    //if (config.fast_switching) {
        //peek(selection->window->hwnd); // TODO Doesn't work yet. See config.fast_switching and peek()
    //}

    /*
    // Alt+`
    if (should_switch) {
        show_hwnd(selection->window->hwnd, true); // TODO? Atm, 'ignore_minimized' is handled in filter(), which means that 'ignore_minimized' affects both Alt+` and Alt+Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.)
    }
    // Alt+Tab
    if (!should_switch) {
        size_gui();
        redraw(&gdi, hMainWindow, selection);
    }
    if (open_gui) {
        int delay = once() ? 0 : config.gui_delay;
        show_gui(delay);
    }
    */
}

static void select_done(selection_t *selection, bool should_switch, bool should_restore) {
    if (should_switch && selection->active) {
        if (!should_restore) {
            // TODO When fast_switching is implemented, it should do something here
            show_hwnd(selection->window->hwnd, true);
            /* dbg */ print(L"done\n");
        } else {
            show_hwnd(selection->restore->hwnd, true);
            /* dbg */ print(L"done (esc)\n");
        }
    }
    hide_gui();
    select_null(selection);
}


static void redraw(gdi_t *gdi, void *hwnd, selection_t *selection) {
    // TODO Resolve circular dep more elegantly
    static linked_window_t *find_first(linked_window_t *window, bool top_level);

    // NOTE Previously gdi_init():

    RECT rect;
    GetClientRect(hwnd, &rect);

    if (!EqualRect(&gdi->rect, &rect)) {
        print(L"new hbm\n");

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

    // NOTE Previously gdi_redraw():

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
        linked_window_t *top1 = find_first(window, false);
        linked_window_t *top2 = find_first(selection->window, false);
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

        linked_window_t *top1 = find_first(window, false);
        linked_window_t *top2 = find_first(selection->window, false);

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

            // TODO Adjust centered text to align with left-most and right-most edges of window
            //print(L"%d, %d, %d, %d\n", rc.left, rc.top, rc.right, rc.bottom);
            if (rc.left < 0) {
                //rc.right += abs(rc.left);
                //rc.left = 0;
                //print(L"%d, %d, %d, %d\n", rc.left, rc.top, rc.right, rc.bottom);
            } else if (rc.right > rect.right) {
                //rc.left -= rc.right - rect.right; // TODO Fix up messy code: rc is modified rect, rect is "client rect"
                //rc.right = rect.right;
                //print(L"%d, %d, %d, %d\n", rc.left, rc.top, rc.right, rc.bottom);
            }

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


static bool get_key(keyboard_t *keyboard, vkcode key) {
    return BITTEST(keyboard->keys, key);
}

static bool set_key(keyboard_t* keyboard, vkcode key, bool down) {
    bool already_down = get_key(keyboard, key);
    if (down) {
        BITSET(keyboard->keys, key);
    } else {
        BITCLEAR(keyboard->keys, key);
    }
    return already_down && down; // return whether this was a key repeat
}


static void autorun(bool enabled, wchar_t *reg_key_name) {

    // TODO For callsite ergonomics, take "args" (below) as function parameter instead

    // Assemble the target (filepath + args) for the autorun reg key

    wchar_t path[MAX_PATH]; // path
    wchar_t args[] = L"--autorun"; // args
    wchar_t target[sizeof path + sizeof args]; // path + args

    GetModuleFileNameW(NULL, path, sizeof path / sizeof * path); // get filepath of current module
    StringCchPrintfW(target, sizeof target / sizeof * target, L"\"%s\" %s", path, args); // "sprintf"

    HKEY reg_key;
    LSTATUS success; // BUG Not checking 'success' below

    if (enabled) {
        success = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_SET_VALUE, NULL, &reg_key, NULL);
        success = RegSetValueExW(reg_key, reg_key_name, 0, REG_SZ, target, sizeof target);
        success = RegCloseKey(reg_key);
    } else {
        success = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &reg_key);
        success = RegDeleteValueW(reg_key, reg_key_name);
        success = RegCloseKey(reg_key);
    }
}


static intptr_t LLKeyboardProc(int nCode, intptr_t wParam, intptr_t lParam) {
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
            select_done(&selection, false, false);
            return pass_message;
        } else {
            // pass-through to Alt+Tab below
        }
    }

    // ========================================
    // Activation
    // ========================================
    // Alt+Tab
    if (key1_down && mod1_held && config.switch_apps) {
        if (!selection.active) {
            rebuild(&windows);
            select_foreground(&selection);
        }
        select_next(&selection, true, shift_held, !config.wrap_bump || !key_repeat);
        size_gui();
        redraw(&gdi, hMainWindow, &selection);
        int delay = once() ? 0 : config.gui_delay;
        show_gui(delay);

        //intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        return consume_message; // Since we "own" this hotkey system-wide, we consume this message unconditionally
    }
    // Alt+`
    if (key2_down && mod2_held && config.switch_windows) {
        if (!selection.active) {
            rebuild(&windows);
            select_foreground(&selection);
        }
        select_next(&selection, false, shift_held, !config.wrap_bump || !key_repeat);
        show_hwnd(selection.window->hwnd, true);

        //intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        return consume_message; // Since we "own" this hotkey system-wide, we consume this message unconditionally
    }
    // ========================================
    // Navigation
    // ========================================
    // Alt+arrows. People ask for this, so...
    if (selection.active && (next_down || prev_down) && mod1_held) { // selection.active: You can't *start* window switching with Alt+arrows, but you can navigate the switcher with arrows when switching is active
        select_next(&selection, true, prev_down, !config.wrap_bump || !key_repeat);

        //intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        return consume_message; // Since our swicher is active, we "own" this hotkey, so we consume this message
    }
    // ========================================
    // Deactivation
    // ========================================

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
        select_done(&selection, true, false);
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message
    }
    if (selection.active && mod2_up) {
        select_done(&selection, true, false);
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message
    }
    // Alt+Enter
    if (selection.active && (mod1_held || mod2_held) && enter_down) {
        select_done(&selection, true, false);
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message
    }
    // Alt+Esc
    if (selection.active && (mod1_held || mod2_held) && esc_down) { // BUG There's a bug here if mod1 and mod2 aren't both the same key. In that case we'll still pass through the key event, even though we should own it since it's not Alt+Esc
        select_done(&selection, true, true);
        //intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        return consume_message; // We don't "own"/monopolize Alt+Esc. It's used by Windows for another, rather useful, window switching mechaninsm, and we preserve that
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
    switch (message) {
        case WM_ERASEBKGND: {
            return 1;
        }
        case WM_ACTIVATE: {
            if (wParam == WA_INACTIVE) {
                //print(L"Wnd Deactivated\n");
                select_done(&selection, false, false);
                return DefWindowProcW(hwnd, message, wParam, lParam);
            } else {
                //print(L"Wnd Activated\n");
                return DefWindowProcW(hwnd, message, wParam, lParam);
            }
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
        case WM_CLOSE: {
            select_done(&selection, false, false);
            int result = MessageBoxW(NULL, L"Do you want to quit cmdtab?", L"cmdtab", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL);
            switch (result) {
                case IDYES: return DefWindowProcW(hwnd, message, wParam, lParam);
                case IDNO: return 0;
            }
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        default: {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }
}

//==============================================================================
// Windows API main()
//==============================================================================

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {

    // Quit if another instance of cmdtab is already running
    hMutexHandle = CreateMutexW(NULL, TRUE, L"cmdtab_mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        int result = MessageBoxW(NULL, L"cmdtab.exe is already running.", L"cmdtab", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
        return result; // 0
    }

    // Prompt about autorun (unless we got autorun param)
    if (wcscmp(pCmdLine, L"--autorun")) { // lstrcmpW, CompareStringW
        int result = MessageBoxW(NULL, L"Start cmdtab.exe automatically? (Relaunch cmdtab.exe to change your mind.)", L"cmdtab", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL);
        switch (result) {
            case IDYES: autorun(true, L"CmdTab");
            case IDNO: autorun(false, L"CmdTab");
        }
    }

    // Set default settings
    defaults(&config);

    // Keep instance handle
    hMainInstance = hInstance;

    // Install keyboard hook
    hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, (HOOKPROC)LLKeyboardProc, NULL, 0);

    // Create window
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = (WNDPROC)WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"cmdtab";

    hMainWindow = CreateWindowExW(WS_EX_TOOLWINDOW, RegisterClassExW(&wcex), NULL, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

    // Use dark mode to make the title bar dark
    // The alternative is to disable the title bar but this removes nice rounded
    // window shaping.
    // For dark mode details, see: https://stackoverflow.com/a/70693198/659310
    BOOL USE_DARK_MODE = TRUE;
    DwmSetWindowAttribute(hMainWindow, DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE, sizeof USE_DARK_MODE);

    // Start msg loop for thread
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        DispatchMessageW(&msg);
    }

    // Done
    UnhookWindowsHookEx(hKeyboardHook);
    ReleaseMutex(hMutexHandle);
    CloseHandle(hMutexHandle);

    return (int)msg.wParam;
}
