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


#define MAX_LOADSTRING 100

// Global Windows variables
void *hInst;
void *hKeyboardHook;
void *hMainWnd;

// Forward declarations of functions included in this code module
intptr_t CALLBACK WndProc(void*, unsigned, intptr_t, intptr_t);
intptr_t CALLBACK LowLevelKeyboardProc(int, intptr_t, intptr_t);
void     CALLBACK TimerProc(void*, unsigned, unsigned, unsigned long);


int APIENTRY wWinMain(void*, void*, wchar_t*, int);

int wWinMain(void *hInstance, void *hPrevInstance, wchar_t *lpCmdLine, int nCmdShow) {
    
    //UNREFERENCED_PARAMETER(hPrevInstance);
    //UNREFERENCED_PARAMETER(lpCmdLine);

    //AllocConsole();
    //void *hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    //wchar_t wszBuffer[] = L"qweąęėšų\n";
    //int len;
    //WriteConsoleW(hConsoleOutput, wszBuffer, sizeof(wszBuffer)/sizeof(wchar_t), &len, NULL);
    OutputDebugStringW(L"Ẅęļçǫɱę ťỡ ûňîĉôɗě\n");

    // Initialize global strings
    //wchar_t szTitle[MAX_LOADSTRING]; // the title bar text
    wchar_t szWindowClass[MAX_LOADSTRING]; // the main window class name
    
    //LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CMDTAB4, szWindowClass, MAX_LOADSTRING);
    
    /*
    // Register the window class
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

    // Register the window class
    unsigned short class = RegisterClassExW(&(WNDCLASSEXW) {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .cbClsExtra = {0},
        .cbWndExtra = {0},
        .hInstance = hInstance,
        .hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CMDTAB4)),
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW+1),
        .lpszMenuName = {0},//MAKEINTRESOURCEW(IDC_CMDTAB4),
        .lpszClassName = szWindowClass,
        .hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SMALL))
    });
    if (class == 0) {
        return false;
    }
    // Create window
    int w = 320;
    int h = 240;
    int x = GetSystemMetrics(SM_CXSCREEN) / 2 - (w / 2);
    int y = GetSystemMetrics(SM_CYSCREEN) / 2 - (h / 2);
    hMainWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW, // "Tool window style" is explicitly not included in ALT-TAB, yay!
        szWindowClass,
        L"", // No title bar text
        0,
        /*
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        */
        x,
        y,
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
    HACCEL *hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDC_CMDTAB4));
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    BOOL bSuccess = UnhookWindowsHookEx(hKeyboardHook);
    if (!bSuccess) {
        return false;
    }
    hKeyboardHook = NULL;

    return (int)msg.wParam;
}

#pragma comment(lib, "shlwapi.lib") 
#pragma comment(lib, "dwmapi.lib") 

#include <wchar.h>     // We're wiiiide, baby!
#include <stdarg.h>    // Used by print()
#include "debugapi.h"  // Used by print() -- ehh: "To use this function [OutputDebugStringW], you must include the Windows.h header in your application (not debugapi.h)." (docs)
#include "WinUser.h"   // Used by title(), path(), filter(), collect(), next()
#include "processthreadsapi.h" // Used by path()
#include "WinBase.h"   // Used by path()
#include "handleapi.h" // Used by path()
#include "Shlwapi.h"   // Used by name()
//#include "corecrt_wstring.h" // Used by name(), included by wchar.h
//#include "dwmapi.h"    // Used by filter()

#include <intrin.h> // Used by debug(), crash()
#define debug() __debugbreak() // Or DebugBreak() in debugapi.h
#define crash() __fastfail(FAST_FAIL_FATAL_APP_EXIT)

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

//
// Char-based bitarray, lovely implementation from https://c-faq.com/misc/bitsets.html
// 
// usage:
//   char bit_array[BITSIZE(42)]; // gives you 42 bits (rounded up to CHAR_BIT) to work with
//   BITSET(bit_array, 69);       // set bit no. 69 to 1
//   BITCLEAR(bit_array, 420);    // set bit no. 420 to 0
//   if (BITTEST(bit_array, 666)) // check if bit no. 666 is set to 1
//
#include <limits.h>	   /* for CHAR_BIT */
#define BITSIZE(n)     ((n + CHAR_BIT - 1) / CHAR_BIT)
#define BITINDEX(b)    ((b) / CHAR_BIT)
#define BITMASK(b)     (1 << ((b) % CHAR_BIT))
#define BITSET(a, b)   ((a)[BITINDEX(b)] |=  BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITINDEX(b)] &= ~BITMASK(b))
#define BITTEST(a, b)  ((a)[BITINDEX(b)] &   BITMASK(b))

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
    wchar_t text[(MAX_PATH*1)+1];
    bool ok;
};

/*
struct link {
    linked_window_t *previous;
    linked_window_t *next;
};
*/

struct linked_window {
    path_t exe_name; // dbg: Not strictly needed to be stored in memory, but it's nice to have available for the debugger and printwindow()-ing while debugging
    title_t title; // dbg: Not strictly needed to be stored in memory, but it's nice to have available for the debugger and printwindow()-ing while debugging
    void *hwnd;
    //link_t apps; // Only top windows have this link set. See link() for a definition of "top window"
    //link_t subwindows; // These are windows of the same app (i.e. same exe_path), called "sub windows"
    linked_window_t *next_sub_window;
    linked_window_t *prev_sub_window;
    linked_window_t *next_top_window; // Only top windows have this link set. See link() for a definition of "top window"
    linked_window_t *prev_top_window; // Only top windows have this link set. See link() for a definition of "top window"
    path_t exe_path; // dbg: Not strictly needed to be stored in memory, could just query & compare at point of need (in link()), but it's nice to have available for the debugger and printwindow()-ing while debugging
};

// Structs for app state

struct windows {
    linked_window_t array[256]; // How many (filtered!) windows do you really need?!
    int count;
};

struct keyboard {
    char keys[32]; // 32x char = 256 bits. Could use CHAR_BIT for correctness, but does Windows (8, I believe is our earliest target) even run on any machine where CHAR_BIT != 8?
};

struct config { // See defaults() for more notes
    // Bindings in vkcode
    unsigned long mod1;
    unsigned long key1;
    unsigned long mod2;
    unsigned long key2;
    // Options
    bool fast_switching; // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
    bool ignore_minimized; // NOTE Currently only observed for subwindow switching BUG! Move ignore_minimized handling from show() -> selectnext(). TODO It seems this should be called 'restore_minimized'?
    unsigned show_delay; // Rename flicker_prevention/delay?
};

struct selection {
    // Wrap in a struct cuz we prefer passing around a pointer meant for mutation in a struct rather than the raw pointer,
    // in part because we prefer the overall look of the arrow operator amidst other code over the dereference operator:
    // 
    // selection->window = next_window 
    // vs. 
    // *selection = next_window
    // 
    // A better reason would have been: The selected window has different semantics than the linked windows in the windows array.
    // There are different functions below that do different things with the selected window than what the functions for the
    // linked windows in the windows array do with them. And so the (crucial) semantic difference warrants a type pun and
    // increases clarity (to wit: note the signalling of difference in intent between functions below that take 'linked_window_t'
    // vs. functions that take 'selection_t')
    linked_window_t *window; // Rename 'current'?
    // Oh look how easy it was to extend the selection semantics when it was already encapsulated in a struct!
    linked_window_t *restore; // Rename 'initial'?
    // Off-load the burden from 'selection->window == NULL' as marker of whether we're pre-init or post-init
    bool active;
};

// Global variables for app state. Yes we could throw these into a 'struct app', but that encourages passing around too much state/context

static windows_t windows;// Filtered windows, as returned by EnumWindows() and filtered by filter()

static keyboard_t keyboard;

static config_t config;

static selection_t selection;

//static bool active; // Off-load the burden from 'selection->window == NULL' as marker of whether we're pre-init or post-init


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
    // sizeof: I'm sorry but that's just how you do it in C. BUG / TODO Wait, does size need -1 here?
    title_t title = {
        .length = MIN(sizeof ((title_t *)0)->text / sizeof((title_t *)0)->text[0], GetWindowTextLengthW(hwnd)) + 1, // Size [in characters!]. +1 for NUL
        .text = {0},
        .ok = {0}
    };
    title.ok = GetWindowTextW(hwnd, &title.text, title.length) > 0; // Length param: "The maximum number of characters to copy to the buffer, including the null character." (docs)
    return title;
}

static path_t path(void *hwnd) {
    // sizeof: I'm sorry but that's just how you do it in C. BUG / TODO Wait, does size need -1 here?
    path_t path = {
        .length = sizeof ((path_t *)0)->text / sizeof ((path_t *)0)->text[0], // NOTE: Used as buffer size (in) [in characters! (docs)] and path length (out) by QueryFullProcessImageNameW()
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
    //
    // Compare paths from last to first char since unique paths nevertheless often have long identical prefixes
    // 
    // The normal way would be:
    // return (path1.length == path2.length && wcscmp(path1.path, path2.path) == 0);
    //
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
        .length = {0},
        .text = {0},
        .ok = {0}
    };
    // sizeof: I'm sorry but that's just how you do it in C. BUG / TODO Wait, does size need -1 here?
    errno_t cpy_error = wcscpy_s(&name.text, sizeof ((path_t *)0)->text, PathFindFileNameW(&path->text)); // BUG Not guarding on 'cpy_error'
    PathRemoveExtensionW(&name.text); //BUG Deprecated, use PathCchRemoveExtension()
    name.length = wcslen(&name.text);
    name.ok = (cpy_error == 0);
    return name;
}


static void show(void *hwnd, bool restore) {
    if (hwnd == GetForegroundWindow()) { // Should this really be here... Where should the responsibility lie of not trying to show the same window again?
        return;
    }
    // Restore from minimized if requested && neccessary
    if (restore && IsIconic(hwnd)) { // TODO Remove this comment? TODO Add config var to ignore minimized windows?? Nah...
        bool windowWasPreviouslyVisible = ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        /* dbg */ print(L"show (restored)\n");
    } else {
        SetForegroundWindow(hwnd);
        /* dbg */ print(L"show\n");
    }
}

static void peek(void *hwnd) {
    //
    // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
    //
    
    //bool windowGotShown = !ShowWindow(hwnd, SW_SHOWNA);
    bool success = SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

}

static void toggle(bool show, unsigned delay) {
    // Toggle hMainWnd (our switcher window/GUI) visibility, with delayed show functionality for quick 
    // switching without indecipherably flashing the GUI. We don't support delayed hiding (why would we?)

    // Delayed show functionality, 
    if (show && delay > 0) { // We don't support delayed hiding (why?), thus check for 'show'
        //unsigned id = show ? 1 : 2; // We don't support delayed hiding (why?), thus only have one timer id: 1
        uintptr_t id = SetTimer(hMainWnd, 1, delay, TimerProc); // Will call toggle(true, 0) after 'delay'
        return;
    } else {
        // Timers automatically repeat, so kill'em
        // We're hiding *now*, so kill any pending show-timers
        KillTimer(hMainWnd, 1);
        //KillTimer(hMainWnd, 2); // We don't support delayed hiding (why?), thus only have one timer id: 1
    }

    // Toggle visibility & set position of window to center
    if (show) {
        int w = 320;
        int h = 240;
        int x = GetSystemMetrics(SM_CXSCREEN) / 2 - (w / 2);
        int y = GetSystemMetrics(SM_CYSCREEN) / 2 - (h / 2);
        SetWindowPos(hMainWnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
        SetFocus(hMainWnd);
    } else {
        ShowWindow(hMainWnd, SW_HIDE);
    }
}

// Rename 'togglenow' or 'shownow'? This is not a general timer handler.
static void TimerProc(void *hwnd, unsigned message, uintptr_t id, unsigned long time) {
    //if (hwnd == hMainWnd) {
        //KillTimer(hwnd, id); // Timers automatically repeat, so kill this one. NOT NECESSARY because toggle(false, X) kills this timer
        toggle(true, 0);
        //KillTimer(hMainWnd, 2); // We don't support delayed hiding (why?), thus only have one timer id: 1
        //toggle((id == 1), 0); // We don't support delayed hiding (why?), thus only have one timer id: 1
    //} else {
        //print(L"WTF!?");
    //}
    /*
    bool show = (id == 1);
    //if (id == 1) show = true;
    //if (id == 2) show = false;
    print(L"%u, %u, %lu\n", message, id, time);
    if (hwnd == hMainWnd) {
        toggle(show, 0);
        KillTimer(hwnd, id);
    }
    */
}


static bool filter(void *hwnd) {

    // Window must be visible
    if (IsWindowVisible(hwnd) == false)
        return false;

    // Ignore windows without title text
    //if (GetWindowTextLengthW(hwnd) == 0)
        //return false;

    // Ignore desktop window
    //if (GetShellWindow() == hwnd)
        //return false;

    // Ignore ourselves (we don't need to, because atm we only load windows and filter them while our window is not visible). Also, we're a WS_EX_TOOLWINDOW (see below)
    //if (hwnd == hMainWnd) // TODO After nailing down the other filters, test if this is necessary anymore
        //return false;

    //int cloaked = 0;
    //int success = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked); // Windows 8
    //if (success && cloaked)
        //return false;

    // "A tool window does not appear in the taskbar or in the dialog that appears when the user presses ALT+TAB." (docs)
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
        return false;

    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE)
        return false;

    //if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST)
        //return false;

    //if (config.ignore_minimized && IsIconic(hwnd))
        //return false;

    //WINDOWPLACEMENT placement = {0};
    //bool success = GetWindowPlacement(hwnd, &placement);
    //RECT rect = placement.rcNormalPosition;
    //int width = rect.right - rect.left;
    //int height = rect.bottom - rect.top;
    //int size = width + height;
    //if (size < 100) // window is less than 100 pixels
        //return false;
        
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

        //* dbg */ print(L"+ %s %s\n", exe_name.text, window_title.text);
    } else {
        //* dbg */ print(L"- %s %s\n", exe_name.text, window_title.text);
    }
}

static void collect(windows_t *windows) {
    // Function names
    // 'enumerate', 'fetch', 'populate', 'build', 'gather', 'collect', 'addall'

    windows->count = 0;
    //* dbg */ print(L"EnumWindows (all windows) (+ means Alt-Tab window, - means not Alt-Tab window)\n");
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
            return; // BUG Use assert, i.e. crash()
        }
        // Determine if window2 is either a) of the same app as a prior top window (and therefore is a sub window), or b) the first window of its app in the window list (i.e. a top window)
        while (window1) {
            if (patheq(window1->exe_path, window2->exe_path)) {
                while (window1->next_sub_window) { window1 = window1->next_sub_window; } // Get last subwindow of window1 (since window1 is a top window)
                window1->next_sub_window = window2;
                window2->prev_sub_window = window1;
                break;
            }
            if (window1->next_top_window == NULL) { // Cheeky but works
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

static void printwindow(linked_window_t *window, bool extended) {
    // TODO Maybe get rid of this function? It just sits so ugly between all these nice functions: add, collect, link, dump, load...
    if (window) {
        if (extended) {
            // Don't look in here
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
        print(L"%s \"%s\" - %s\n", window->exe_name.text, window->title.text, window->exe_path.text);
    } else {
        print(L"NULL window\n");
    }
}

static void dump(windows_t *windows, bool fancy) {
    double mem = (double)(sizeof *windows + sizeof keyboard + sizeof config + sizeof selection) / 1024; // TODO 'keyboard', 'config' and 'selection' are referencing global vars
    //print(L"________________________________\n");
    print(L"%i Alt-Tab windows (%.0fkb mem):\n", windows->count, mem);
    if (!fancy) {
        // Just dump the array
        for (int i = 0; i < windows->count; i++) {
            linked_window_t *window = &windows->array[i];
            print(L"%i ", i);
            printwindow(window, true);
        }
    } else {
        // Dump the linked list
        linked_window_t *top_window = &windows->array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
        linked_window_t *sub_window = top_window->next_sub_window;
        while (top_window) {
            print(L"+ ");
            printwindow(top_window, false);
            while (sub_window) {
                print(L"\t- ");
                printwindow(sub_window, false);
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

static void update(windows_t *windows) {
    // We're starting from scratch so go get y'all windows
    collect(windows);
    link(windows);
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
    if (top_window) { // Could call self (recursion) but looks cleaner this way:
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
        while (window->prev_top_window) { window = window->prev_top_window; } // 2. Go to first top widndow
    } else {
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window (which is also technically "first sub window")
    }
    // return window; // TODO WTF How does this work in findnext() without return value!?
} 

static linked_window_t *last(linked_window_t *window, bool top_window) {
    if (top_window) { // Could use first() but looks cleaner this way:
        while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
        while (window->next_top_window) { window = window->next_top_window; } // 2. Go to last top window
    } else {
        while (window->next_sub_window) { window = window->next_sub_window; } // 1. Go to last sub window
    }
    // return window; // TODO WTF How does this work in findnext() without return value!?
}

// Can't use function name 'next'?? 'after'?!
// TODO / BUG: Handle NULL window, i.e. there are NO windows to switch to
static linked_window_t *findnext(linked_window_t *window, bool top_window, bool reverse_direction, bool allow_wrap) {

    //if (window == NULL) {
        //debug();
        //return window; // TODO Maybe too defensive. Check call hierarchy and semantics for if this is ever a possillity
    //}

    if (top_window) {
        // Get top app window in case 'window' is a sub window
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
        /* dbg */ print(L"wrap%s\n", reverse_direction ? L" (to last)" : L" (to first)");
        return reverse_direction ? last(window, top_window) : first(window, top_window);
    }
    // Can't be NULL, or we'll have crashed in this function already (NULL pointer dereference above)
    return window;
}

// These, 'defaults' & 'once', should maybe be up with the raw window function ('show', 'peek' & 'toggle')
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
        .fast_switching = false, // BUG / TODO Doesn't work. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
        .ignore_minimized = true, // NOTE Currently only observed for subwindow switching and BUG! Move ignore_minimized handling from show() -> selectnext()
        .show_delay = 150
    };
}

static void once(void) { // aka. init()?
    defaults(&config);
}

static bool o_selectnext(selection_t *selection, bool app, bool reverse, bool wrap) { // bool show? for showing switcher // rename fast to switch? whoops can't, keyword
// Can't use function name 'switch' or 'select' :(    
    if (!selection->window) {
        update(&windows); // Update to current windows and window order
        /* dbg */ dump(&windows, true);
        linked_window_t *foreground = find(&windows, GetForegroundWindow()); // BUG! What do when no foreground window find, because foreground window filtered out??
        selection->window = foreground;
        if (!selection->restore) {
            selection->restore = foreground;
        }
        if (!foreground) {
            /* dbg */ print(L"whoops, we can't find the foreground window");
            return false; // There are no windows (akshually, none of the filtered windows is the foreground window)
        } else {
            /* dbg */ print(L"foreground window: ");
            /* dbg */ printwindow(selection->window, false);
        }
    }

    linked_window_t *next_window = findnext(selection->window, app, reverse, wrap); // TODO / BUG: Handle NULL window, i.e. there are NO windows to switch to
    if (selection->window != next_window) { // BUG! Can't [formulate bug here]  Should this really be here... Where should the responsibility lie of not trying to show the same window again?
        selection->window = next_window;
        return true;
    } else {
        return false; // We did nothing, so let low level hook know to pass the message on
    }

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
static void o_activate(selection_t *selection) {
    //if (selection->active) {
        //return;
    //}
        
    // Update to current windows and window order
    update(&windows);
    
    /* dbg */ dump(&windows, true);
        
    linked_window_t *foreground = find(&windows, GetForegroundWindow()); // BUG! What do when no foreground window find, because foreground window filtered out??
    selection->window = foreground;
    selection->restore = foreground;
    selection->active = true; // TODO Should be (foreground != NULL)

    //if (!selection->restore) {
        //selection->restore = foreground;
    //}
    if (!foreground) {
        /* dbg */ print(L"whoops, we can't find the foreground window");
        return false; // There are no windows (akshually, none of the filtered windows is the foreground window)
    } else {
        /* dbg */ print(L"foreground window: ");
        /* dbg */ printwindow(selection->window, false);
    }

    return (foreground == NULL);
}
static void o_reset(selection_t *selection) {
    // Rename 'deactivate'?
    selection->active = false;
    selection->restore = NULL;
    selection->window = NULL;
}
static void setrestore(linked_window_t *restore) {
    selection.restore = restore;
    print(L"restore ");
    printwindow(selection.restore, false);
}
static void o_apps(selection_t *selection, bool reverse, bool repeat) {
    // Rename 'switchapp', 'selectnextapp', 'nextapp', 'selectapp', 'cycleapps'
    if (!selectnext(selection, true, reverse, !repeat)) {
        return;
    }

    /* dbg */ // Damn that looks scary xD (it's not tho--it's just getting and adding together the size of the 'text' fields of 'path_t' and 'title_t')
    /* dbg */ wchar_t app_title[sizeof ((path_t *)0)->text + sizeof ((title_t *)0)->text +8];
    /* dbg */ errno_t cpy_error;
    /* dbg */ cpy_error = wcscpy_s(app_title, sizeof ((path_t *)0)->text, selection->window->exe_name.text);
    /* dbg */ cpy_error = wcscat_s(app_title, 32, L" - "); // Danger! Count your characters and size! We've allocated +8 for this, which is 4 wchars (3 for string, 1 for NUL)
    /* dbg */ cpy_error = wcscat_s(app_title, sizeof ((title_t *)0)->text, selection->window->title.text);
    /* dbg */ SetWindowTextW(hMainWnd, app_title); //selection->window->exe_name.text);

    /* dbg */ print(L"next app%s: ", repeat ? L" (repeat)" : L"");
    /* dbg */ printwindow(selection->window, false);

    if (config.fast_switching) {
        peek(selection->window->hwnd); // TODO Doesn't work yet. See config.fast_switching and peek()
    }
    //setrestore(selection->window);
    toggle(true, config.show_delay);
}
static void o_windows(selection_t *selection, bool reverse, bool repeat) {
    if (!selectnext(selection, false, reverse, !repeat)) {
        return;
    }

    /* dbg */ // Damn that looks scary xD (it's not tho--it's just getting and adding together the size of the 'text' fields of 'path_t' and 'title_t')
    /* dbg */ wchar_t app_title[sizeof ((path_t *)0)->text + sizeof ((title_t *)0)->text + 1 + 8];
    /* dbg */ errno_t cpy_error;
    /* dbg */ cpy_error = wcscpy_s(app_title, sizeof ((path_t *)0)->text, selection->window->exe_name.text);
    /* dbg */ cpy_error = wcscat_s(app_title, 32, L" - "); // Danger! Count your characters and size! We've allocated +8 for this, which is 4 wchars (3 for string, 1 for NUL)
    /* dbg */ cpy_error = wcscat_s(app_title, sizeof ((title_t *)0)->text, selection->window->title.text);
    /* dbg */ SetWindowTextW(hMainWnd, app_title); //selection->window->exe_name.text);

    /* dbg */ print(L"next subwindow%s: ", repeat ? L" (repeat)" : L"");
    /* dbg */ printwindow(selection->window, false);

    show(selection->window->hwnd, config.ignore_minimized); // BUG! Move ignore_minimized handling from show() -> selectnext()
}

static void select_null(selection_t *selection) {
    selection->window = NULL;
    selection->restore = NULL;
    selection->active = false;
}

static void select_foreground(selection_t *selection) {
    linked_window_t *foreground = find(&windows, GetForegroundWindow()); // BUG! What do when no foreground window find, because foreground window filtered out??
    selection->window = foreground;
    selection->restore = foreground;
    selection->active = (foreground != NULL);

    if (!foreground) {
        // There are no windows (akshually, none of the filtered windows is the foreground window)
        /* dbg */ print(L"whoops, we can't find the foreground window");
    } else {
        /* dbg */ print(L"foreground window: ");
        /* dbg */ printwindow(selection->window, false);
    }
}

static bool select_next(selection_t *selection, bool cycle_apps, bool reverse_direction, bool allow_wrap) {
    linked_window_t *next_window = findnext(selection->window, cycle_apps, reverse_direction, allow_wrap); // TODO / BUG: Handle NULL window, i.e. there are NO windows to switch to
    if (selection->window != next_window) { // BUG! Can't [formulate bug here]  Should this really be here... Where should the responsibility lie of not trying to show the same window again?
        selection->window = next_window;
        return true;
    } else {
        return false;
    }
}

static void cycle(selection_t *selection, bool cycle_apps, bool reverse_direction, bool allow_wrap, bool show_gui, bool instant_switch) { // 'instant' / 'immediate'... 'commit'?! Well, bro, what do you propose when you can't use 'switch' or 'show'?
    if (!selection->active) {
        update(&windows);
        /* dbg */ dump(&windows, true);
        select_foreground(selection);
    }

    if (!select_next(selection, cycle_apps, reverse_direction, allow_wrap)) {
        return;
    }

    /* dbg */ // Damn that looks scary xD (it's not tho--it's just getting and adding together the size of the 'text' fields of 'path_t' and 'title_t')
    /* dbg */ wchar_t app_title[sizeof((path_t *)0)->text + sizeof((title_t *)0)->text + 1 + 8];
    /* dbg */ errno_t cpy_error;
    /* dbg */ cpy_error = wcscpy_s(app_title, sizeof((path_t *)0)->text, selection->window->exe_name.text);
    /* dbg */ cpy_error = wcscat_s(app_title, 32, L" - "); // Danger! Count your characters and size! We've allocated +8 for this, which is 4 wchars (3 for string, 1 for NUL)
    /* dbg */ cpy_error = wcscat_s(app_title, sizeof((title_t *)0)->text, selection->window->title.text);
    /* dbg */ SetWindowTextW(hMainWnd, app_title); //selection->window->exe_name.text);

    /* dbg */ print(L"next %s%s: ", cycle_apps ? L"app" : L"subwindow", allow_wrap ? L"" : L" (repeat)");
    /* dbg */ printwindow(selection->window, false);

    //if (config.fast_switching) {
        //peek(selection->window->hwnd); // TODO Doesn't work yet. See config.fast_switching and peek()
    //}
    //setrestore(selection->window);

    // Alt+`
    if (instant_switch) {
        show(selection->window->hwnd, config.ignore_minimized); // BUG! Move ignore_minimized handling from show() -> selectnext()
    }
    // Alt+Tab
    if (show_gui) {
        toggle(true, config.show_delay); // BUG This doesn't check if gui is already visible...
    }

}

static bool done(selection_t *selection, bool hide_gui, bool restore) {
    if (!selection->active) {
        return false;
    }

    if (hide_gui) {
        toggle(false, 0);
    }
    
    if (restore) {
        show(selection->restore->hwnd, true); // Fixed BUG This doesn't check if selection->window->hwnd is the same as the foreground window, so it potentially shows a already shown window. Maybe check for that in show()?
    } else {
        // TODO When fast_switching is implemented, it should do something here
        show(selection->window->hwnd, true); // Fixed BUG This doesn't check if selection->window->hwnd is the same as the foreground window, so it potentially shows a already shown window. Maybe check for that in show()?
    }

    select_null(selection);

    /* dbg */ print(L"done\n");

    return true;
}

static void _mod1up(selection_t *selection) {
    setrestore(NULL);
    if (selection->window) {
        toggle(false, 0);
        // TODO When fast_switching is implemented, it should do something here
        show(selection->window->hwnd, true); // Fixed BUG This doesn't check if selection->window->hwnd is the same as the foreground window, so it potentially shows a already shown window. Maybe check for that in show()?
        selection->window = NULL;
        /* dbg */ print(L"clear (mod1up)\n");
    }
}
static void _mod2up(selection_t *selection) {
    //if (selection->window) {
        selection->window = NULL;
        setrestore(NULL);
        /* dbg */ print(L"clear (mod2up)\n");
    //}
}
static bool _mod_esc(selection_t *selection) {
    
    // TODO Restore? Not working yet. 'Restore' is part of fast_switching, to restore the original window (or actually the whole Z-Order)
    //          when cancelling switching while in fast_switching mode
    //if (config.fast_switching && selection->restore) {
    //    show(selection.restore, true);
    //}

    bool consume = false;
    
    if (selection->window) {
        selection->window = NULL;
        toggle(false, 0);
        consume = true;
    }
    if (selection->restore) {
        show(selection->restore->hwnd, true); // Fixed BUG This doesn't check if selection->window->hwnd is the same as the foreground window, so it potentially shows a already shown window. Maybe check for that in show()?
        setrestore(NULL);
        consume = true;
    }

    /* dbg */ print(L"cancel (alt+esc)\n");
    return consume;
}

// TODO Rewrite this comment and put it where it should be
// keyboard->keys
// 
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
    bool key_repeat = setkey(&keyboard, virtual_key, key_down); // Manually track key repeat using 'keyboard->keys' (see above)

    ///* dbg */ print(L"virtual_key:%llu\tscanCode:%llu\tdown:%d\trepeat:%d\n", virtual_key, scanCode, bKeyDown, bKeyRepeat);

    // Typically:
    //  mod1+key1 = Alt+Tab
    //  mod2+key2 = Alt+` (scancode 0x29)
    bool shift_held = getkey(&keyboard, VK_LSHIFT); // NOTE: This is the *left* shift key
    bool mod1_held  = getkey(&keyboard, config.mod1);
    bool mod2_held  = getkey(&keyboard, config.mod2);
    bool key1_down   = virtual_key == config.key1 && key_down;
    bool key2_down   = virtual_key == config.key2 && key_down;
    bool mod1_up     = virtual_key == config.mod1 && !key_down;
    bool mod2_up     = virtual_key == config.mod2 && !key_down;
    bool esc_down    = virtual_key == VK_ESCAPE && key_down;
    bool prev_down   = (virtual_key == VK_UP   || virtual_key == VK_LEFT)  && key_down;
    bool next_down   = (virtual_key == VK_DOWN || virtual_key == VK_RIGHT) && key_down;
    // TODO Maybe separate up/left & down/right, and use left/right for apps & up/down for windows?
    // Especially since I think we're gonna graphically put a vertical window title list under the horizontal app icon list

    // NOTE: By returning a non-zero value from the hook procedure, the message is consumed and does not get passed to the target window
    intptr_t pass_message = 0; // i.e. 'false'
    intptr_t consume_message = 1; // i.e. 'true'

    // Prevent the need to keep checking all over the code if functions are being called pre-init or post-init (usually by checking if selection->window == NULL or not)
    //static bool activated = false;

    // Alt+Tab
    if (key1_down && mod1_held) {
        intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        ///* dbg */ print(L"mod1key1\n");
        //activate(&selection);
        //apps(&selection, shift_held, key_repeat);
        cycle(&selection, true, shift_held, !key_repeat, true, false);
        return consume_message; // Since we "own" this hotkey system-wide, we consume this message unconditionally
    }
    // Alt+`
    if (key2_down && mod2_held) {
        //if (!activated) { activate(); activated = true; }
        intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        ///* dbg */ print(L"mod2key2\n");
        //windows(&selection, shift_held, key_repeat);
        cycle(&selection, false, shift_held, !key_repeat, false, true);
        return consume_message; // Since we "own" this hotkey system-wide, we consume this message unconditionally
    }
    // Alt+arrows. People ask for this, so...
    if (selection.active && ((next_down || prev_down) && mod1_held)) { // activated: You can't start window switching with Alt+arrows, but you can navigate the switcher with arrows when the switcher is showing
        intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
        ///* dbg */ print(L"alt+arrows\n");
        //apps(&selection, prev_down, key_repeat);
        cycle(&selection, true, shift_held, !key_repeat, false, false);
        return consume_message; // Since we "own" this hotkey (our swicher is showing), we consume this message unconditionally
    }
    // Alt keyup
    // BUG! Aha! Lemme tell yalittle secret: Our low level hook doesn't observe "Alt keydown" because Alt can be pressed down for
    //      so many purposes on system level. We're only interested in Alt+Tab, which means that we only really check for "Tab keydown",
    //      and then transparently check if Alt was already down when Tab was pressed.
    //      This means that we let all "Alt keydown"s pass through our hook, even the "Alt keydown" that the user pressed right before
    //      pressing Tab, which is sort of "ours", but we passed it through anyways (can't consume it after-the-fact).
    //      The result is that there is a hanging "Alt down" that got passed to the foreground app which needs an "Alt keyup", or
    //      the Alt key gets stuck. Thus, we always pass through "Alt keyup"s, even if we our app used it for some functionality.
    //      In short:
    //      1) Our app never hooks Alt keydowns
    //      2) Our app hooks Alt keyups, but always passes it through on pain of getting a stuck Alt key because of point 1
    if (selection.active && mod1_up) {
        ///* dbg */ print(L"mod1up\n");
        //mod1up(&selection);
        done(&selection, true, false);
        //activated = false;
        ///* dbg */ print(L"________________________________\n");
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message
    }
    if (selection.active && mod2_up) { // BUG There's unmodelled behavior if mod1 & mod2 are the same key. ATM, in that case, only mod1up() is called and not mod2up()
        ///* dbg */ print(L"mod2up\n");
        //mod2up(&selection);
        done(&selection, false, false);
        //activated = false;
        ///* dbg */ print(L"________________________________\n");
        return pass_message; // See note above on "Alt keyup" on why we don't consume this message
    }
    // Alt+Esc
    if (selection.active && ((mod1_held || mod2_held) && esc_down)) {
        ///* dbg */ print(L"alt+esc\n");
        //bool result = mod_esc(&selection);
        bool result = done(&selection, true, true);
        //activated = false;
        ///* dbg */ print(L"________________________________\n");
        return result; // We don't "own"/monopolize Alt+Esc. It's used by Windows for another, rather useful, window switching mechaninsm, and we preserve that
    }
    // Sink. Consume all other keystrokes when we're activated. Maybe make this a config option in case shit gets weird with the switcher eating everything when we're activated
    if (selection.active && (mod1_held || mod2_held)) {
        //if (activated /* selection.window */) {
            /* dbg */ print(L"sink vkcode:%llu%s%s\n", virtual_key, key_down ? L" down" : L" up", key_repeat ? L" (repeat)" : L"");
            return consume_message;
        //} else {
            //return pass_message;
        //}
    }
    // TODO Support:
    //   [x] Arrow keys
    //   [ ] Q for (Q)uit application (like macOS)
    //   [ ] W for close (W)indow (like macOS)
    //   [ ] Alt-F4
    //   [ ] Mouse click app icon to switch
    //   [ ] Cancel switcher on mouse click outside

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}



#define WM_USER_TRAYICON WM_USER + 1

intptr_t CALLBACK About(void *, unsigned, intptr_t, intptr_t);

intptr_t CALLBACK WndProc(void *hwnd, unsigned message, intptr_t wParam, intptr_t lParam) {
    switch (message) {
        case WM_CREATE:
        {
            once();
            //update(&windows);
            //dump(&windows, false); // dbg
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


            //hwnd hwnd = GetForegroundWindow();
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
                    DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_ABOUTBOX), hwnd, About);
                    break;
                case IDM_EXIT:
                    DestroyWindow(hwnd);
                    break;
                default:
                    return DefWindowProcW(hwnd, message, wParam, lParam);
            }
        } break;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hwnd, &ps);
            break;
        } 
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        } 
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return 0;
}

intptr_t CALLBACK About(void *hWnd, unsigned message, intptr_t wParam, intptr_t lParam) {
    // Message handler for about box.
    switch (message) {
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
