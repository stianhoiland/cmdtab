// cmdtab4.cpp : Defines the entry point for the application.
//


/*
#define W32(r) __declspec(dllimport) r __stdcall

W32(int)       MultiByteToWideChar(unsigned, int, char *, int, wchar_t *, int);
W32(int)       WideCharToMultiByte(unsigned, int, wchar_t *, int, char *, int, char *, int *);
W32(void *)    GetStdHandle(int);
W32(int)       WriteFile(void *, void *, int, int *, void *);
W32(int)       WriteConsoleW(void *, wchar_t *, int, int *, void *);
W32(void *)    CreateFileW(wchar_t *, int, int, void *, int, int, void *);
W32(int)       CloseHandle(void *);
W32(void *)    CreateFileMappingA(void *, void *, int, int, int, char *);
W32(wchar_t *) GetCommandLineW(void);
W32(int)       GetConsoleMode(void *, int *);
W32(int)       GetEnvironmentVariableW(const wchar_t *, wchar_t *, int);
W32(int)       GetFileSize(void *, int *);
W32(int)       GetModuleFileNameW(void *, wchar_t *, int);
W32(void *)    MapViewOfFile(void *, int, int, int, size_t);
W32(void *)    VirtualAlloc(void *, size_t, int, int);
W32(void)      ExitProcess(int); // __declspec(noreturn)
*/

//#pragma comment(lib,"user32.lib") 

#define _UNICODE
#define UNICODE
#include <stdbool.h>

#include "framework.h"
#include "cmdtab4.h"

//#pragma comment(lib, "shlwapi.lib")     // Link to this file.

#define MAX_LOADSTRING 100

// Global variables
void *hInst;
void *hKeyboardHook;
void *hMainWnd;

// Forward declarations of functions included in this code module
intptr_t CALLBACK WndProc(void*, unsigned int, intptr_t, intptr_t);
intptr_t CALLBACK LowLevelKeyboardProc(int, intptr_t, intptr_t);


int APIENTRY wWinMain(void*, void*, wchar_t*, int);

int wWinMain(void *hInstance, void *hPrevInstance, wchar_t *lpCmdLine, int nCmdShow) {
    
    //UNREFERENCED_PARAMETER(hPrevInstance);
    //UNREFERENCED_PARAMETER(lpCmdLine);

    //AllocConsole();
    //void *hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    //wchar_t wszBuffer[] = L"qweąęėšų\n";
    //int len;
    //WriteConsoleW(hConsoleOutput, wszBuffer, sizeof(wszBuffer)/sizeof(wchar_t), &len, NULL);
    OutputDebugStringW(L"qweąęėšų\n");

    // Initialize global strings
    //wchar_t szTitle[MAX_LOADSTRING]; // the title bar text
    wchar_t szWindowClass[MAX_LOADSTRING]; // the main window class name
    
    //LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CMDTAB4, szWindowClass, MAX_LOADSTRING);
    
    // Register the window class
    /*
    WNDCLASSEXW wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CMDTAB4));
    wcex.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_CMDTAB4);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SMALL));
    
    RegisterClassExW(&wcex);
    */
    
    unsigned short class = RegisterClassExW(&(WNDCLASSEXW) {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .cbClsExtra = 0,
        .cbWndExtra = 0,
        .hInstance = hInstance,
        .hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CMDTAB4)),
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW+1),
        .lpszMenuName = MAKEINTRESOURCEW(IDC_CMDTAB4),
        .lpszClassName = szWindowClass,
        .hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SMALL))
    });
    if (class == 0) {
        return false;
    }
    // Perform application initialization
    int w = 320;
    int h = 240;
    hMainWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        szWindowClass,
        L"", // No title bar text
        0,
        /*
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        */
        GetSystemMetrics(SM_CXSCREEN)/2 - (w/2),
        GetSystemMetrics(SM_CYSCREEN)/2 - (h/2),
        w,
        h,
        NULL,
        NULL,
        hInstance,
        NULL
    );
    if (!hMainWnd) {
        return false;
    }
    // This removes the window title bar, window shadow and window shape rounding
    //SetWindowLong(hMainWnd, GWL_STYLE, GetWindowLong(hMainWnd, GWL_STYLE)&~WS_CAPTION);



    hInst = hInstance; // Used by WndProc()
    //ShowWindow(hMainWnd, nCmdShow);
    //UpdateWindow(hMainWnd);


    hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    if (!hKeyboardHook) {
        return false;
    }

    
    // Main message loop
    void *hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDC_CMDTAB4));
    MSG message;
    while (GetMessageW(&message, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(message.hwnd, hAccelTable, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    int success = UnhookWindowsHookEx(hKeyboardHook);
    if (!success) {
        return false;
    }
    hKeyboardHook = NULL;

    return (int)message.wParam;
}

#pragma comment(lib, "shlwapi.lib") 
#pragma comment(lib, "dwmapi.lib") 

#include <wchar.h> // We're wiiiide, baby!
#include <stdarg.h> // Used by print()
#include "debugapi.h" // Used by print()
#include "WinUser.h" // Used by title(), path(), filter(), collect(), next()
#include "processthreadsapi.h" // Used by path()
#include "WinBase.h" // Used by path()
#include "handleapi.h" // Used by path()
#include "Shlwapi.h" // Used by name()
//#include "corecrt_wstring.h" // Used by name(), included by wchar.h
#include "dwmapi.h" // Used by filter()

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct path path_t;
typedef struct title title_t;
//typedef struct link link_t;
typedef struct linked_window linked_window_t;
typedef struct windows windows_t;
typedef struct keyboard keyboard_t;
typedef struct config config_t;
typedef struct selection selection_t;

struct title {
    int length; // GetWindowTextW() wants 'int'
    wchar_t text[(MAX_PATH*2)+1];
    bool ok;
};

struct path {
    unsigned long length; // QueryFullProcessImageNameW() wants 'unsigned long'
    wchar_t text[MAX_PATH+1];
    bool ok;
};

/*
struct link {
    linked_window_t *previous;
    linked_window_t *next;
};
*/

struct linked_window {
    path_t exe_name; // dbg
    title_t title; // dbg
    void *hwnd;
    //link_t apps; // Only top windows have this link set. See link() for a definition of "top window"
    //link_t subwindows; // These are windows of the same app (i.e. same exe_path), called "sub windows"
    linked_window_t *next_sub_window;
    linked_window_t *prev_sub_window;
    linked_window_t *next_top_window; // Only top windows have these
    linked_window_t *prev_top_window; // Only top windows have these
    path_t exe_path; // dbg: Not strictly needed to be stored in memory, could just query & compare at point of need (in link()), but it's nice to have available for printwindowing while debugging
};

// App state

struct windows {
    linked_window_t array[256];
    int count;
};

struct keyboard {
    char keys[32]; // 32x char = 256 bits. Could use CHAR_BIT for correctness, but does Windows (8, I believe is our earliest target) even run on any machine where CHAR_BIT != 8?
};

struct config { // See defaults() for notes
    // bindings in vkcode
    unsigned long mod1;
    unsigned long key1;
    unsigned long mod2;
    unsigned long key2;
    // options
    bool fast_switching; // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
    bool ignore_minimized; // Currently only observed for subwindow switching and BUG! Move ignore_minimized handling from show() -> selectnext()
};

struct selection {
    linked_window_t *app;
    linked_window_t *subwindow;
    linked_window_t *restore;
    bool switching;
};


// Global variables for app state. Yes we could throw these into a 'struct app', but that encourages passing around too much context

static windows_t windows;// = {0}; // Filtered windows, as returned by EnumWindows() and filtered by filter()

static keyboard_t keyboard;// = {0};

static config_t config;// = {0};

static selection_t selection;// = {0};


static void print(wchar_t *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    wchar_t wcs[4096];
    vswprintf_s(wcs, 4096, fmt, argp);
    va_end(argp);
    OutputDebugStringW(wcs);
}


static title_t title(void *hwnd) {
    title_t title = {
        .length = MIN((MAX_PATH * 2), GetWindowTextLengthW(hwnd)),
        .text = {0},
        .ok = false
    };
    title.ok = GetWindowTextW(hwnd, &title.text, title.length + 1) > 0; // +1 bcoz: "The maximum number of characters to copy to the buffer, including the null character." (docs)
    return title;
}

static path_t path(void *hwnd) {
    path_t path = {
        .length = MAX_PATH, // NOTE: Used as buffer size (in) and path length (out) by QueryFullProcessImageNameW()
        .text = {0},
        .ok = false
    };
    //
    // How to get the file path of the executable for the process that owns the window:
    // 
    // 1. GetWindowThreadProcessId() to get process id for window
    // 2. OpenProcess() for access to remote process memory
    // 3. QueryFullProcessImageNameW() [Windows Vista] to get full "executable image" name (i.e. exe path) for process
    //
    unsigned long pid;
    GetWindowThreadProcessId(hwnd, &pid);
    void *process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid); // See GetProcessHandleFromHwnd()
    if (!process) {
        print(L"ERROR couldn't open process for window: %s\n", title(hwnd).text);
        return path;
    } else {
        bool success = QueryFullProcessImageNameW(process, 0, path.text, &path.length); // Can use GetLongPathName() if getting shortened paths
        if (!success) {
            print(L"ERROR couldn't get exe path for window: %s\n", title(hwnd).text);
        }
        CloseHandle(process);
        path.ok = success;
        return path;
    }
}

static bool patheq(path_t path1, path_t path2) {
    // Compare paths from last to first char since unique paths nevertheless often have identical prefixes
    // The normal way would be:
    //return (path1.length == path2.length && wcscmp(path1.path, path2.path) == 0);

    if (path1.length != path2.length) {
        return false;
    }
    for (int i = path1.length - 1; i >= 0; i--) {
        if (path1.text[i] != path2.text[i]) {
            return false;
        }
    }
    return true;
}

static path_t name(path_t *path) {
    path_t name = {
        .length = 0,
        .text = {0},
        .ok = true
    };
    errno_t error = wcscpy_s(&name.text, MAX_PATH, PathFindFileNameW(&path->text)); // BUG Not checking 'error' return
    PathRemoveExtensionW(&name.text); //BUG Deprecated, use PathCchRemoveExtension()
    name.length = wcslen(&name.text);
    return name;
}


static void show(void *hwnd, bool restore/*, bool hide_switcher*/) {
    // Show window, restore from minimized if neccessary
    //void *hwnd = window->hwnd;
    if (restore && IsIconic(hwnd)) {  // TODO Add config switch to ignore minimized windows?? Nah...
        bool windowGotShown = !ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        print(L"show (restored)\n");
    } else {
        SetForegroundWindow(hwnd);
        print(L"show\n");
    }

    /*
    // Hide switcher?
    int switcherGotHidden = ShowWindow(hMainWnd, SW_HIDE);
    return switcherGotHidden;
    if (switcherGotHidden) {
        print(L"hide: alt keyup\n");
        //return consume_message; // Consume message BUG: Alt key gets stuck if we consume this message, but this also seems to propogate an Alt keydown event to the underlying window?
    } else {
        // The hook caught Alt keyup, but the switcher window was already hidden
        // Since this is a no-op for us, we pass the message on
        //return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
    */
}

static void peek(void *hwnd) {
    //
    // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
    //
    
    //bool windowGotShown = !ShowWindow(hwnd, SW_SHOWNA);
    bool success = SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

}


static bool filter(void *hwnd) {

    // Window must be visible
    if (IsWindowVisible(hwnd) == false)
        return false;

    // Ignore windows without title text
    //if (GetWindowTextLengthW(hwnd) == 0)
        //return false;

    // Ignore desktop window
    if (GetShellWindow() == hwnd)
        return false;

    // Ignore ourselves
    if (hwnd == hMainWnd) // TODO After nailing down the other filters, test if this is necessary anymore
        return false;

    int cloaked = 0;
    int success = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked); // Windows 8
    if (success && cloaked)
        return false;

    //if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
        //return false;

    //if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)
        //return false;

    //if (config.ignore_minimized && IsIconic(hwnd))
        //return false;

    WINDOWPLACEMENT placement = {0};
    success = GetWindowPlacement(hwnd, &placement);
    RECT rect = placement.rcNormalPosition;
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    int size = width + height;
    if (size < 100) // window is less than 100 pixels
        return false;
        

    /*
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

static void add(void *hwnd, void *context) {
    if (filter(hwnd)) {
        windows_t *windows = (windows_t *)context;

        path_t exe_path = path(hwnd);
        path_t exe_name = name(&exe_path);
        title_t window_title = title(hwnd);
        
        windows->array[windows->count++] = (linked_window_t){
            .exe_name = exe_name, // dbg
            .title = window_title, // dbg
            .hwnd = hwnd,
            .next_sub_window = NULL,
            .prev_sub_window = NULL,
            .next_top_window = NULL,
            .prev_top_window = NULL,
            .exe_path = exe_path // dbg. See note in 'struct linked_window' declaration
        };

        //print(L"+ %s %s\n", exe_name.text, window_title.text);
    } else {
        //print(L"- %s %s\n", exe_name.text, window_title.text);
    }
}

static void collect(windows_t *windows) {
    // Names
    // 'enumerate', 'fetch', 'populate', 'build', 'gather', 'collect', 'addall'

    windows->count = 0;
    //print(L"EnumWindows (all windows) (+ means Alt-Tab window, - means not Alt-Tab window)\n");
    EnumWindows(add, windows);
}

static void link(windows_t *windows) {
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
            print(L"whoops, bad exe paths?!");
            return; // BUG Use assert
        }
        // Determine if window2 is either a) same app as a prior top window (a sub window), or b) first window of its app in the list (a top window)
        while (window1) {
            if (patheq(window1->exe_path, window2->exe_path)) {
                while (window1->next_sub_window) { window1 = window1->next_sub_window; } // Get last sub window of window1 (window1 is a top window)
                window1->next_sub_window = window2;
                window2->prev_sub_window = window1;
                break;
            }
            if (window1->next_top_window == NULL) { // Cheeky but works
                break;
            }
            window1 = window1->next_top_window; // Check next top window
        }
        if (window2->prev_sub_window == NULL) { // window2 was not assigned a previous window of the same app, thus it is a top window
            window2->prev_top_window = window1;
            window1->next_top_window = window2;
        }
    }
}

static void printwindow(wchar_t *prefix, linked_window_t *window) {
    // TODO Maybe get rid of this function? It just sits so ugly between all these nice functions: add, collect, link, dump, load...
    if (window) {
        print(L"%s %s  %s %s\n", prefix ? prefix : L"Window:", window->exe_name.text, window->title.text, window->exe_path.text);
    } else {
        print(L"whoops! window is NULL! %s\n", prefix ? prefix : L"");
    }
}

static void dump(windows_t *windows, bool fancy) {
    double mem = (double)(sizeof * windows + sizeof config + sizeof selection) / 1024; // TODO 'config' and 'selection' are referencing global vars
    print(L"%i Alt-Tab windows (%.0fkb mem):\n", windows->count, mem);
    if (!fancy) {
        // Just dump the array
        for (int i = 0; i < windows->count; i++) {
            linked_window_t *window = &windows->array[i];
            print(L"%i ", i);
            printwindow(L"", window);
        }
    } else {
        // Dump the linked list
        linked_window_t *top_window = &windows->array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
        linked_window_t *sub_window = top_window->next_sub_window;
        while (top_window) {
            printwindow(L"+", top_window);
            while (sub_window) {
                printwindow(L"\t-", sub_window);
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

static void load(windows_t *windows) {
    // We're starting from scratch so go get y'all windows
    collect(windows);
    link(windows);
    dump(windows, true); // dbg
}

static linked_window_t *find(windows_t *windows, void *hwnd) {
    for (int i = 0; i < windows->count; i++) {
        if (windows->array[i].hwnd == hwnd) {
            return &windows->array[i];
        }
    }
    return NULL;
}

static linked_window_t *first(linked_window_t *window, bool top) {
    if (top) { // Could call self (recursion) but looks cleaner this way:
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
        while (window->prev_top_window) { window = window->prev_top_window; } // 2. Go to first top widndow
    } else {
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window (which is also technically "first sub window")
    }
    // return window; // WTF How does this work in findnext() without return value!?
} 

static linked_window_t *last(linked_window_t *window, bool top) {
    if (top) { // Could use first() but looks cleaner this way:
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
        while (window->next_top_window) { window = window->next_top_window; } // 2. Go to last top window
    } else {
        while (window->next_sub_window) { window = window->next_sub_window; } // 1. Go to last sub window
    }
    // return window; // WTF How does this work in findnext() without return value!?
}


// Can't use function name 'next'?? 'after'?!
// TODO / BUG: Handle NULL window, i.e. there are NO windows to switch to
static linked_window_t *findnext(linked_window_t *window, bool top, bool reverse, bool wrap) {

    linked_window_t *next_window = NULL; // Could use ternary, but this looks clearer:

    if (top && !reverse) {
        next_window = window->next_top_window;
    }
    if (top && reverse) {
        next_window = window->prev_top_window;
    }
    if (!top && !reverse) {
        next_window = window->next_sub_window;
    }
    if (!top && reverse) {
        next_window = window->prev_sub_window;
    }
    
    if (next_window) {
        return next_window;
    }

    bool alone;

    if (top) {
        alone = !window->next_top_window && !window->prev_top_window; // All alone on the top :(
    } else {
        alone = !window->next_sub_window && !window->prev_sub_window; // Top looking for sub ;)
    }

    if (!next_window && wrap && !alone) {
        print(L"wrap to %s\n", reverse ? L"last" : L"first");// from %s %s\n", window->exe_name.text, window->title.text);
        return reverse ? last(window, top) : first(window, top);
    }
    // Can't be NULL, or we'll have crashed in this function already (NULL pointer dereference above)
    return window;
    //return NULL;
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
        .fast_switching = false,
        .ignore_minimized = true
    };
}

static void once(void) { // aka. init()?
    defaults(&config);
}


static void toggle(bool show) {
    if (show) {
        int w = 320;
        int h = 240;
        int x = GetSystemMetrics(SM_CXSCREEN) / 2 - (w / 2);
        int y = GetSystemMetrics(SM_CYSCREEN) / 2 - (h / 2);
        SetFocus(hMainWnd);
        SetWindowPos(hMainWnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
    } else {
        ShowWindow(hMainWnd, SW_HIDE);
    }
}

static bool selectforeground(selection_t *selection, windows_t *windows) {
    linked_window_t *foreground = find(windows, GetForegroundWindow());
    selection->app = foreground;
    selection->subwindow = foreground;
    selection->restore = foreground;
    printwindow(L"foreground app:", foreground);
    return (foreground != NULL);
}

/*
static void selectnull(selection_t *selection) {
    selection->restore = NULL;
    selection->subwindow = NULL;
    selection->app = NULL;
}
*/

/*
static bool done(bool show, bool restore) {
    // Restore?
    if (show && selection.restore) {
        //show(selection.restore);
    }
    // Clear selection state
    selection = (selection_t){0};
}

static bool cancel(bool restore) {
    // Restore?
    if (restore && selection.restore) {
        show(selection.restore);
    }
    // Clear selection state
    selection = (selection_t){0};
}
*/

// Can't use function name 'switch' :(
static bool selectnext(selection_t *selection, bool app, bool reverse, bool wrap) { // bool show? for showing switcher // rename fast to switch? whoops can't, keyword
    
    if (!selection->restore) {
        load(&windows); // Update to current windows and window order
        if (!selectforeground(selection, &windows)) {
            return false; // There are no windows (akshually, none of the filtered windows is the foreground window)
        }
    }

    if (app) {
        linked_window_t *next_window = findnext(selection->app, app, reverse, wrap); // TODO / BUG: Handle NULL window, i.e. there are NO windows to switch to
        if (next_window == selection->app) { return false; } // We did nothing, so let low level hook know to pass the message on
        selection->app = next_window;
        selection->subwindow = next_window; // Advance the app that we're (potentially) cycling subwindows for
    } else {
        linked_window_t *next_window = findnext(selection->subwindow, app, reverse, wrap); // TODO / BUG: Handle NULL window, i.e. there are NO windows to switch to
        if (next_window == selection->subwindow) { return false; } // We did nothing, so let low level hook know to pass the message on
        selection->subwindow = next_window;
    }

    return true;

    /*
    linked_window_t **selected = app ? &selection.app : &selection.subwindow;
    linked_window_t *next = findnext(*selected, app, reverse, !repeat);
    if (!next) {
        return false; // TODO Remove error semantics. This function should be void return. Callers should do other checks to determine if a switch happened
    }
    if (*selected != next) {
        *selected = next;

        if (fast && config.fast_switching) {
            show((*selected)->hwnd, true);
        }

        // Show main window // bool show? for showing switcher
        int windowGotShown = !ShowWindow(hMainWnd, SW_SHOW);
        if (windowGotShown) {
            UpdateWindow(hMainWnd); // TODO / BUG: Is this necessary? It's here since the template code included it when showing window
        }
        SetForegroundWindow(hMainWnd);
    }
    return true;
    */
}

static bool mod1key1(selection_t *selection, bool reverse, bool repeat) {
    if (!selectnext(selection, true, reverse, !repeat)) {
        // We did nothing, but we'll let the low level hook know to consume the message (return true) (despite what it says in selectnext())
        // since we "own" the hotkey (whether or not we do anything, i.e. even when we do nothing, nothing *else* should happen--it's our hotkey)
        return true; 
    }

    selection->switching = true;

    // GUI stuff:
    //...

    if (config.fast_switching) {
        peek(selection->app->hwnd); // Doesn't work yet. See config.fast_switching and peek()
    } 

    toggle(true);
    SetWindowTextW(hMainWnd, selection->app->title.text); // dbg

    //show(selection->app->hwnd, true); // mod1 is for app switching. We don't switch immediately onn mod1key1, we switch on mod1up

    print(L"next app%s: %s %s\n", repeat ? L" (repeat)" : L"", selection->app->exe_name.text, selection->app->title.text);

    return true;
}
static void mod1up(selection_t *selection) {
    if (selection->switching) {
        toggle(false);
    }
    if (selection->app && selection->app != selection->restore) { // && !fast_switching
        show(selection->app->hwnd, true); // mod1 is for app switching. We don't switch immediately onn mod1key1, we switch on mod1up
    }
    if (selection->restore) {
        selection->restore = NULL;
        selection->subwindow = NULL;
        selection->app = NULL;
        //*selection = (selection_t){0};
        print(L"clear: mod1up\n");
    }
}

static bool mod2key2(selection_t *selection, bool reverse, bool repeat) {
    if (!selectnext(selection, false, reverse, !repeat)) {
        // We did nothing, but we'll let the low level hook know to consume the message (return true) (despite what it says in selectnext())
        // since we "own" the hotkey (whether or not we do anything, i.e. even when we do nothing, nothing *else* should happen--it's our hotkey)
        return true;
    }

    //selection->switching = true;
    //selection->switching = false;

    // GUI stuff:
    //...

    // mod2 is for subwindow switching. We don't switch on mod2up, we switch immediately on mod2key2
    show(selection->subwindow->hwnd, config.ignore_minimized); // BUG! Move ignore_minimized handling from show() -> selectnext()

    print(L"next subwindow%s: %s %s\n", repeat ? L" (repeat)" : L"", selection->subwindow->exe_name.text, selection->subwindow->title.text);

    return true; 
}
static void mod2up(selection_t *selection) {
    //if (selection->switching) {
        //toggle(false);
    //}

    if (selection->subwindow && selection->subwindow != selection->restore) { // && !fast_switching
        //show(selection->subwindow->hwnd, true); // mod2 is for subwindow switching. We don't switch on mod2up, we switch immediately on mod2key2
    }
    if (selection->restore) {
        selection->restore = NULL;
        selection->subwindow = NULL;
        selection->app = NULL;
        //*selection = (selection_t){0};
        print(L"clear: mod2up\n");
    }
}

static bool mod_esc(selection_t *selection) {
    
    // Restore? Not working yet. 'Restore' is part of fast_switching, to restore the original window (or actually the whole Z-Order)
    //          when cancelling switching while in fast_switching mode
    //if (config.fast_switching && selection->restore) {
    //    show(selection.restore, true);
    //}
    
    // Clear selection state
    if (selection->restore) {
        selection->restore = NULL;
        selection->subwindow = NULL;
        selection->app = NULL;
        toggle(false); // BUG Refactor out of this condition into a selection->switching condition
        print(L"cancel: alt+esc\n");
        return true;
    } else {
        return false; // We don't "own"/monopolize Alt+Esc, because it's used by the OS for another, rather useful, window switching mechaninsm
    }

}

//
// keyboard->keys
// 
// Char-based bitarray, lovely implementation from https://c-faq.com/misc/bitsets.html
// usage:
//   char bit_array[BITSIZE(42)]; // gives you 42 bits (+/- CHAR_BIT) to work with
//   BITSET(bit_array, 69);       // set bit no. 69 to 1
//   BITCLEAR(bit_array, 420);    // set bit no. 420 to 0
//   if (BITTEST(bit_array, 666)) // check if bit no. 666 is set to 1
#include <limits.h>	   /* for CHAR_BIT */
#define BITSIZE(n)     ((n + CHAR_BIT - 1) / CHAR_BIT)
#define BITINDEX(b)    ((b) / CHAR_BIT)
#define BITMASK(b)     (1 << ((b) % CHAR_BIT))
#define BITSET(a, b)   ((a)[BITINDEX(b)] |=  BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITINDEX(b)] &= ~BITMASK(b))
#define BITTEST(a, b)  ((a)[BITINDEX(b)] &   BITMASK(b))

// Manually track modifier state and key repeat with a bit array (key repeat must be tracked manually in low level hook)
// vkCode: "The code must be a value in the range 1 to 254." (docs)
// 256 bits for complete tracking of currently held virtual keys (vkCodes, i.e. VK_* defines).
// vkCode technically fits in a 'char' (see above), but we'll use the same type as the struct
static bool setkey(keyboard_t *keyboard, unsigned long vkcode, bool down) {
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
static bool getkey(keyboard_t *keyboard, unsigned long vkcode) {
    return BITTEST(keyboard->keys, vkcode);
}

intptr_t LowLevelKeyboardProc(int nCode, intptr_t wParam, intptr_t lParam) {
    if (nCode < 0 || nCode != HC_ACTION) { // "If code is less than zero, the hook procedure must pass the message to blah blah blah read the docs"
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // Info about the keyboard for the low level hook
    KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;
    unsigned long virtual_key = kb->vkCode;
    bool key_down = !(kb->flags & LLKHF_UP);
    // Manually track key repeat using 'keyboard->keys' as a bit array (see above)
    bool key_repeat = setkey(&keyboard, virtual_key, key_down);

    //print(L"virtual_key:%llu\tscanCode:%llu\tdown:%d\trepeat:%d\n", virtual_key, scanCode, bKeyDown, bKeyRepeat);

    // Typically:
    //  mod1+key1 = Alt+Tab
    //  mod2+key2 = Alt+` (scancode 0x29)
    bool shift_held = getkey(&keyboard, VK_LSHIFT); // NOTE: This is the *left* shift key
    bool mod1_held  = getkey(&keyboard, config.mod1);
    bool mod2_held  = getkey(&keyboard, config.mod2);
    bool key1_down   = virtual_key == config.key1 && key_down;
    bool key2_down   = virtual_key == config.key2 && key_down;
    bool key1_repeat = key1_down && key_repeat;
    bool key2_repeat = key2_down && key_repeat;
    bool mod1_up     = virtual_key == config.mod1 && !key_down;
    bool mod2_up     = virtual_key == config.mod2 && !key_down;
    bool esc_down    = virtual_key == VK_ESCAPE && key_down;

    // NOTE: By returning a non-zero value from the hook procedure, the message is consumed and does not get passed to the target window
    intptr_t pass_message = 0; // i.e. 'false'
    intptr_t consume_message = 1; // i.e. 'true'

    // Alt+Tab
    if (key1_down && mod1_held) {
        intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam);
        return mod1key1(&selection, shift_held, key1_repeat);
    }
    // Alt+`
    if (key2_down && mod2_held) {
        intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam);
        return mod2key2(&selection, shift_held, key2_repeat);
    }
    // Alt keyup
    // BUG! Aha! Lemme tell yalittle secret: Our low level hook doesn't observe "Alt keydown" because Alt can be pressed down for
    //      so many purposes on system level. We're only interested in Alt+Tab, which means that we only really check for "Tab keydown",
    //      and then transparently check if Alt was already down when Tab was pressed.
    //      This means that we let all "Alt keydown"s pass through our hook, even the "Alt down" that the user pressed right before
    //      pressing Tab, which is sort of "ours", but we passed it through anyways (can't consume it after-the-fact).
    //      The result is that there is a hanging "Alt down" that got passed to the foreground app which needs an "Alt keyup", or
    //      the Alt key gets stuck. Thus, we never consume an "Alt up", even if we our app used it for some functionality, but pass
    //      it through so that, just like with all the "Alt keydown"s we pass through, we also pass through all the "Alt keyup"s.
    //      In short:
    //      1) Our app never hooks Alt keydown
    //      2) Our app hooks Alt keyup, but always passes it through on pain of getting a stuck Alt key because of the previous point
    if (mod1_up) {
        mod1up(&selection);
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message
    }
    if (mod2_up) {
        mod2up(&selection);
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message

        /*
        // TODO Testing fast_switching
        if (selected_app_restore) { // && fast_switching
            SetForegroundWindow(selected_app_restore->window);
            selected_app_restore = NULL;
            return 1;
        }
        */

        // Hide window
        int windowGotHidden = ShowWindow(hMainWnd, SW_HIDE);
        if (windowGotHidden) {
            print(L"hide: alt keyup\n");
            //return consume_message; // Consume message BUG: Alt key gets stuck if we consume this message, but this also seems to propogate an Alt keydown event to the underlying window?
        } else {
            // The hook caught Alt keyup, but the switcher window was already hidden
            // Since this is a no-op for us, we pass the message on
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }
    }
    // Alt+Esc
    if ((mod1_held || mod2_held) && esc_down) {
        return mod_esc(&selection);
        //cancel(false);

        /*
        // Hide window
        int windowGotHidden = ShowWindow(hMainWnd, SW_HIDE);
        if (windowGotHidden) {
            print(L"hide: alt+esc\n");
            return consume_message;
        } else {
            // The hook caught Alt+Esc, but the switcher window was already hidden
            // Since this is a no-op for us, we pass the message on
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }
        */
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}



#define WM_USER_TRAYICON WM_USER + 1

intptr_t CALLBACK About(void *, unsigned int, intptr_t, intptr_t);

intptr_t CALLBACK WndProc(void *hWnd, unsigned int msg, intptr_t wParam, intptr_t lParam) {
    switch (msg) {
        case WM_CREATE:
        {
            once();
        }
        break;
        case WM_HOTKEY:
        {
            //print(L"Here's a pointer for you: %" PRIxPTR "\n", wParam);
            print(L"Here: %d\n", wParam);
            //OutputDebugStringW(L"Here's a pointer for you: %" PRIxPTR "\n", wParam);

        }
        break;
        case WM_USER_TRAYICON:
        {

        }
        break;
        case WM_SHOWWINDOW:
        {
            //PROCESS_NAME_WIN32
                //PROCESS_QUERY_INFORMATION

            // enumerate windows


            //HWND hwnd = GetForegroundWindow();
            //wchar_t title[256];
            //GetWindowTextW(hwnd, title, 255);
            //print(L"Foreground: %s\n", title);
        }
        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId) {
                case IDM_ABOUT:
                    DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_ABOUTBOX), hWnd, About);
                    break;
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                default:
                    return DefWindowProcW(hWnd, msg, wParam, lParam);
            }
        } break;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        } break;
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

intptr_t CALLBACK About(void *hWnd, unsigned int msg, intptr_t wParam, intptr_t lParam) {
    // Message handler for about box.
    switch (msg) {
        case WM_INITDIALOG:
            return (intptr_t)true;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hWnd, LOWORD(wParam));
                return (intptr_t)true;
            }
            break;
    }
    return (intptr_t)false;
}
