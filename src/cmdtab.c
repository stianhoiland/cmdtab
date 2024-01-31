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
intptr_t CALLBACK WndProc(void *, unsigned int, intptr_t, intptr_t);
intptr_t CALLBACK LowLevelKeyboardProc(int, intptr_t, intptr_t);

intptr_t CALLBACK About(void *, unsigned int, intptr_t, intptr_t);

int APIENTRY wWinMain(void *, void *, wchar_t *, int);

int APIENTRY wWinMain(void *hInstance, void *hPrevInstance, wchar_t *lpCmdLine, int nCmdShow) {

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
            .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
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
        GetSystemMetrics(SM_CXSCREEN) / 2 - (w / 2),
        GetSystemMetrics(SM_CYSCREEN) / 2 - (h / 2),
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

    /*
    int success = RegisterHotKey(hMainWnd, 1, MOD_ALT | MOD_NOREPEAT, VK_OEM_5); // alt + VK_OEM_5   N: 0x4E
    if (!success) {
        return false;
    }
    success = RegisterHotKey(hMainWnd, 2, MOD_ALT | MOD_NOREPEAT, VK_TAB); // alt + VK_OEM_5   N: 0x4E
    if (!success) {
        return false;
    }
    */




    // Main message loop
    void *hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDC_CMDTAB4));
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    int success = UnhookWindowsHookEx(hKeyboardHook);
    if (!success) {
        return false;
    }
    hKeyboardHook = NULL;

    return (int)msg.wParam;
}

#include <wchar.h>
#include <stdarg.h>

void DebugOut(wchar_t *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    wchar_t dbg_out[4096];
    vswprintf_s(dbg_out, 4096, fmt, argp);
    va_end(argp);
    OutputDebugStringW(dbg_out);
}







#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// function to reverse a string
/*
void wcsrev(wchar_t *wstr, int len) {
    // if the string is empty
    if (!wstr) {
        return;
    }
    // pointer to start and end at the string
    int i = 0;
    int j = len ? len : strlen(wstr)-1;

    // reversing string
    while (i<j) {
        wchar_t c = wstr[i];
        wstr[i] = wstr[j];
        wstr[j] = c;
        i++;
        j--;
    }
}
*/

/* Just a useful little thing to avoid dealing with buffers just to debug print a window's title */
typedef struct title title_t;
struct title {
    int length;
    wchar_t text[MAX_PATH];
    bool ok;
};
title_t title(HWND hwnd) {
    title_t title = {
        .length = MIN(MAX_PATH - 1, GetWindowTextLengthW(hwnd)),
        .text = {0},
        .ok = false
    };
    title.ok = GetWindowTextW(hwnd, title.text, title.length) > 0;
    return title;
}


typedef struct path path_t;
struct path {
    unsigned long length;
    wchar_t text[MAX_PATH];
    bool ok;
};
path_t path(HWND hwnd) {
    //
    // How to get the file path of the executable for the process that owns the window:
    // 
    // 1. GetWindowThreadProcessId() to get process id for window
    // 2. OpenProcess() for access to remote process memory
    // 3. QueryFullProcessImageNameW() [Windows Vista] to get full "executable image" name (i.e. exe path) for process
    //

    // Return result
    path_t path = {
        .length = MAX_PATH, // Used as buffer size (in) and path length (out) by QueryFullProcessImageNameW()
        .text = {0},
        .ok = false
    };

    unsigned long pid;
    GetWindowThreadProcessId(hwnd, &pid);
    void *process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid); // See GetProcessHandleFromHwnd()
    if (!process) {
        DebugOut(L"ERROR couldn't open process for window: %s\n", title(hwnd).text);
        return path;
    } else {
        path.ok = QueryFullProcessImageNameW(process, 0, path.text, &path.length); // Can use GetLongPathName() if getting shortened paths
        if (!path.ok) {
            DebugOut(L"ERROR couldn't get exe path for window: %s\n", title(hwnd).text);
            CloseHandle(process);
            return path;
        } else {
            CloseHandle(process);
            return path;
        }
    }
}
bool patheq(path_t path1, path_t path2) {
    // Compare paths from last to first char since unique paths nevertheless often have identical prefixes
    if (path1.length != path2.length)
        return false;
    for (int i = path1.length - 1; i >= 0; i--) {
        if (path1.text[i] != path2.text[i])
            return false;
    }
    return true;
    // The normal way would be:
    //return (path1.length==path2.length&&wcscmp(path1.path, path2.path)==0);
}
path_t name(path_t *path) {
#pragma comment(lib, "shlwapi.lib") // Link to this file.
#include "shlwapi.h" // for PathFindNextComponent()
    // Return result
    path_t result = {
        .length = 0,
        .text = {0},
        .ok = true
    };
    wcscpy_s(&result.text, (rsize_t)MAX_PATH - 1, PathFindFileNameW(&path->text));
    PathRemoveExtensionW(&result.text); //BUG Deprecated, use PathCchRemoveExtension()
    result.length = (size_t)wcslen(&result.text);
    return result;
}



typedef struct linked_window linked_window_t;

struct linked_window {
    path_t exe_name; // dbg
    title_t title; // dbg
    void *window;
    linked_window_t *next_sub_window;
    linked_window_t *prev_sub_window;
    linked_window_t *next_top_window; // only top windows have these
    linked_window_t *prev_top_window; // only top windows have these
    path_t exe_path; // dbg: Not strictly needed to be stored in memory, could just query & compare at point of need (in linkwindows()), but it's nice to have available for debugging
};

// 10.24kb (not counting .exe_path)
linked_window_t windows[256] = {0};

intptr_t windows_count = 0;

//const struct node NodeNull = {0}; // or just node == (struct node){0}


//struct node 

//typedef struct node window;


//HWND windows[1024];
//HWND apps[128];
//#define MAX_APPS 32
//#define MAX_WINDOWS 32 // This is per app
//char exePaths[MAX_APPS][MAX_WINDOWS][MAX_PATH];

/*
linked_window_t *last_sub_window(linked_window_t *window) {
    while (window->next_sub_window) { window = window->next_sub_window; }
    return window;
}
bool is_top_window(linked_window_t *window)
void add_sub_window(linked_window_t *window, linked_window_t *sub) {
    linked_window_t *last = last_sub_window(window);
    last->next_sub_window = sub;
    sub->prev_sub_window = last;
}
void add_top_window(linked_window_t *window, linked_window_t *top) {
    window2->prev_top_window = window1;
    window1->next_top_window = window2;
}
*/

bool alttab(void *window) {
    if (IsWindowVisible(hwnd) == false)
        return false;

    //if (GetWindowTextLengthW(hwnd) == 0)
        //return false;

    // Ignore desktop window.
    if (GetShellWindow() == hwnd)
        return false;

    //if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
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
void addwindow(void *window, void *context) {
    //wchar_t title[512+1];
    //GetWindowTextW(hwnd, title, 512);

    if (alttab(window)) {
        //wchar_t *file_name = PathFindFileNameW(&path.path);
        //PathRemoveExtensionW(file_name);

        //GetWindowTextW(hwnd, title, 512);
        //DebugOut(L"%s\t\t%s\n", file_name, title);

        //intptr_t *count = (intptr_t *)context;
        path_t exe_path = path(window);
        path_t exe_name = name(&exe_path);
        windows[windows_count++] = (linked_window_t){
            .exe_name = exe_name, // dbg
            .title = title(window), // dbg
            .window = window,
            .next_sub_window = NULL,
            .prev_sub_window = NULL,
            .next_top_window = NULL,
            .prev_top_window = NULL,
            .exe_path = exe_path // dbg. See note in 'struct linked_window' declaration
        };

        return;

        if (windows_count < 2)
            return;

        linked_window_t *window1 = &windows[windows_count - 2];
        linked_window_t *window2 = &windows[windows_count - 1];

        bool same_app;

        if (!window1->exe_path.ok || !window2->exe_path.ok) {
            DebugOut(L"whoops, bad exe paths?!");
        }

        same_app = patheq(window2->exe_path, window1->exe_path);

        if (same_app) {
            window2->prev_sub_window = window1;
            window1->next_sub_window = window2;
        } else {

        }



        linked_window_t *app1;
        linked_window_t *app2;

        // get to top-level app window

        app1 = window1;

        while (app1->prev_sub_window != NULL && !same_app) {
            app1 = app1->prev_sub_window;
        }

        // check if any prev apps are same as window2

        app2 = app1;

        while (app2->prev_top_window != NULL && !same_app) {
            app2 = app2->prev_top_window;
            same_app = patheq(window2->exe_path, app2->exe_path);
        }

        if (same_app) {
            // add to end of this app's windows

            while (app2->next_sub_window != NULL) {
                app2 = app2->next_sub_window;
            }

            window2->prev_sub_window = app2;
            app2->next_sub_window = window2;

        } else {
            // add to end of apps
            window2->prev_top_window = app1;
            app1->next_top_window = window2;
        }

        // check if any next apps are same as window2

        app2 = app1;

        while (app2->next_top_window != NULL && !same_app) {
            app2 = app2->next_top_window;
            same_app = patheq(window2->exe_path, app2->exe_path);
        }

        if (same_app) {
            // add to end of this app's windows

            while (app2->next_sub_window != NULL) {
                app2 = app2->next_sub_window;
            }

            window2->prev_sub_window = app2;
            app2->next_sub_window = window2;
        }



        //DebugOut(L"+ %s\n", title(hwnd).text);
    } else {
        //DebugOut(L"- %s\n", title(hwnd).text);
    }
}
void linkwindows() {
    // Reset global var
    windows_count = 0;
    //DebugOut(L"EnumWindows (all windows) (+ means Alt-Tab window, - means not Alt-Tab window)\n");
    // Populate 'windows' global var
    EnumWindows(addwindow, NULL);
    //EnumWindows(enum_windows_proc, &windows_count);
    //DebugOut(L"%lld Alt-Tab windows\n", windows_count);

    //wchar_t title[512+1]; // Used for debug print
    // As a convenience and (very) slight optimization, we return the foreground window (as a linked_window_t, not a HWND)
    //HWND foreground_window = GetForegroundWindow();
    //linked_window_t *foreground_linked_window = NULL;

    // BUG So many ways to optimize this:
    // [ ] Hash the path
    // [x] Store the path strings reversed so that string comparision start from the last letter of the filename extension and work backwards
    // [ ] Build a tree with nodes for each path component
    // Not optimal but fast enough
    // If two windows have the exact same executable path, then they are counted as windows of the same app
    // Algo: 
    //       1. Take window1 and window2
    //       2. If they are same app AND window1 doesn't already have 'next_sub_window' then:
    //            window1.next_sub_window = window2
    //            window2.prev_sub_window = window1
    //       3. If they are not same app AND window1 is first window of its app in the list (i.e. 'prev_sub_window' == NULL) then:
    //            window1.next_top_window = window2
    //            window2.prev_top_window = window1
    //

    if (windows_count < 2)
        return;

    // A top window is any window that is the first of its app in the windows list
    // A sub window is any window of the same app after that
    for (int i = 1; i < windows_count; i++) {
        linked_window_t *window1 = &windows[0]; // windows[0] is always a top window because it is necessarily the first window of its app in the list
        linked_window_t *window2 = &windows[i];
        if (!window1->exe_path.ok || !window2->exe_path.ok) {
            DebugOut(L"whoops, bad exe paths?!");
        }
        // Determine if window2 is a) same app as a prior top window (a sub window), or b) first window of its app in the list (a top window)
        while (window1) {
            if (patheq(window1->exe_path, window2->exe_path)) {
                while (window1->next_sub_window) { window1 = window1->next_sub_window; } // get last sub window
                window1->next_sub_window = window2;
                window2->prev_sub_window = window1;
                break;
            }
            if (window1->next_top_window == NULL) { // cheeky but works
                break;
            }
            window1 = window1->next_top_window; // check next top window
        }
        if (window2->prev_sub_window == NULL) {
            window2->prev_top_window = window1;
            window1->next_top_window = window2;
        }

        continue;

        linked_window_t *window = &windows[i];

        if (!window->exe_path.ok) {
            DebugOut(L"whoops, bad exe paths?!");
        }
        //
        // windows[0].next_top_window —→ windows[?].next_top_window —→ windows[?].next_top_window —→ windows[?].next_top_window —→ ...
        //        |                      |                      |                   ↑
        //        ↓                      ↓                      ↓                   |
        //  next_sub_window        next_sub_window        next_sub_window     prev_sub_window
        //                                                      ↑
        //                                                      |
        //                                                prev_sub_window
        // 
        //
        // GOD dammit I hate this code, but honest to god I'm not sure there's a better way to
        // express it in code. It's not very complicated--I promise!--it just *looks* so ugly!
        // 
        // Check if the app of the window is already in the app chain (windows[0]->next_top_window->next_top_window->next_top_window...)
        bool unseen = true;
        // Start at first window
        linked_window_t *app = &windows[0];
        // Is first window and this window same app?
        if (patheq(app->exe_path, window->exe_path)) {
            // Yes, so get to end of the app's windows
            while (app->next_sub_window) {
                app = app->next_sub_window;
            }
            // And link the window there
            app->next_sub_window = window;
            window->prev_sub_window = app;
        } else {
            // Not same as first window, so check next apps in the app chain
            while (app->next_top_window) {
                app = app->next_top_window;
                // Is next app and window same app?
                if (patheq(app->exe_path, window->exe_path)) {
                    // Yes, so get to end of this next app's windows
                    while (app->next_sub_window) {
                        app = app->next_sub_window;
                    }
                    // And link the window there
                    app->next_sub_window = window;
                    window->prev_sub_window = app;
                    // Stop checking app chain, the window is a sub-window
                    unseen = false;
                    break;
                }
            }
            // All apps in app chain have been checked, was the window a sub-window or a top-level window?
            if (unseen) {
                // The app of the window was not seen in the app chain, so add it to the end of the app chain
                app->next_top_window = window;
                window->prev_top_window = app;
            }
        }
    }
    return;




    return;

    for (int i = 0; i < windows_count - 1; i++) {
        linked_window_t *window1 = &windows[i];
        linked_window_t *window2 = &windows[i + 1];

        if (!window1->exe_path.ok || !window2->exe_path.ok) {
            DebugOut(L"whoops, bad exe paths?!");
        }

        bool same_app = patheq(window1->exe_path, window2->exe_path);

        if (same_app) {
            window1->next_sub_window = window2;
            window2->prev_sub_window = window1;
        } else {
            bool window2_is_first_app = true;

            // get to top-level app window
            while (window1->prev_sub_window != NULL) {
                window1 = window1->prev_sub_window;
            }
            // check previous top-level app windows (apps)
            while (window1->prev_top_window != NULL) {
                window1 = window1->prev_top_window;

                bool same_app = patheq(window1->exe_path, window2->exe_path);

                if (same_app) {
                    break;
                }
            }
        }
    }
    /*
    for (int i = 0; i < windows_count; i++) {
        //int s = i; // allow skips
        for (int j = i+1; j < windows_count; j++) {
            linked_window_t *window1 = &windows[i];
            linked_window_t *window2 = &windows[j];

            if (!window1->exe_path.ok || !window2->exe_path.ok) {
                DebugOut(L"whoops, bad exe paths?!");
            }

            bool same_app = patheq(window1->exe_path, window2->exe_path);

            if (same_app) {
                if (window1->next_sub_window == NULL) {
                    window1->next_sub_window = window2;
                    window2->prev_sub_window = window1;
                }
            } else {
                if (window1->next_top_window == NULL && window1->prev_sub_window == NULL) { // second cond means window1 is first window of its app in the list
                    window1->next_top_window = window2;
                    window2->prev_top_window = window1;
                    //if (s==j)
                        //s = j;
                }
            }

            // window1 is configured, go to next
            if (window1->next_sub_window != NULL && window1->next_top_window != NULL)
                break;

            //i = s;
        }
        */
        //windows[0].prev_sub_window
        //windows[windows_count-1].

        // As a convenience and (very) slight optimization, we return the foreground window (as a linked_window_t, not a HWND)
        //if (foreground_linked_window == NULL && windows[i].window==foreground_window) {
            //foreground_linked_window = &windows[i];
        //}


        //wchar_t *file_name = PathFindFileNameW(&path.path);
        //PathRemoveExtensionW(file_name);

        //GetWindowTextW(hwnd, title, 512);
        //DebugOut(L"%s\t\t%s\n", file_name, title);

    // Connect first app with last app, creating a cycle
    for (int j = windows_count - 1; j >= 0; j--) {

    }
    //DebugOut(L"lol");
    //return foreground_linked_window;
}
void print_windows(bool flat) {
    DebugOut(L"%lld Alt-Tab windows:\n", windows_count);
    if (flat) {
        // just print the array
        for (int i = 0; i < windows_count; i++) {
            linked_window_t *window = &windows[i];
            DebugOut(L"- %s\n", window->title.text);
        }
        DebugOut(L"\n");
    } else {
        // print the linked list
        linked_window_t *window;
        linked_window_t *subwindow;

        window = &windows[0];
        subwindow = &windows[0];
        DebugOut(L"- %s %s\n", window->exe_name.text, window->title.text);
        while (subwindow->next_sub_window != NULL) {
            subwindow = subwindow->next_sub_window;
            DebugOut(L"\t- %s %s\n", subwindow->exe_name.text, subwindow->title.text);
        }

        window = &windows[0];
        subwindow = &windows[0];
        while (window->next_top_window != NULL) {
            window = window->next_top_window;
            DebugOut(L"- %s %s\n", window->exe_name.text, window->title.text);
            while (subwindow->next_sub_window != NULL) {
                subwindow = subwindow->next_sub_window;
                DebugOut(L"\t- %s %s\n", subwindow->exe_name.text, subwindow->title.text);
            }
        }
        DebugOut(L"\n");
    }
}
linked_window_t *linked_window(HWND hwnd) {
    for (int i = 0; i < windows_count; i++) {
        if (windows[i].window == hwnd) {
            return &windows[i];
        }
    }
    return NULL;
}
linked_window_t *next_top_window(linked_window_t *window, bool reverse, bool wrap) {
    // Prepare
    if (window == NULL) {
        window = linked_window(GetForegroundWindow());
    }
    if (window->prev_sub_window) {
        while (window->prev_sub_window) { window = window->prev_sub_window; }
    }
    // Cycle forward
    if (!reverse && window->next_top_window) {
        return window->next_top_window;
    }
    // Cycle backward
    if (reverse && window->prev_top_window) {
        return window->prev_top_window;
    }
    // Wrap forward
    if (!reverse && !window->next_top_window && wrap) {
        DebugOut(L"wrap from %s %s\n", window->exe_name.text, window->title.text);
        while (window->prev_top_window) { window = window->prev_top_window; }
        return window;
    }
    // Wrap backward
    if (reverse && !window->prev_top_window && wrap) {
        DebugOut(L"wrap from %s %s\n", window->exe_name.text, window->title.text);
        while (window->next_top_window) { window = window->next_top_window; }
        return window;
    }
    // Can be NULL?
    return window;
}
linked_window_t *next_sub_window(linked_window_t *window, bool reverse, bool wrap) {
    // Prepare
    if (window == NULL) {
        window = linked_window(GetForegroundWindow());
    }
    if (window == NULL) {
        DebugOut(L"uuh...");
    }
    // Cycle forward
    if (!reverse && window->next_sub_window) {
        return window->next_sub_window;
    }
    // Cycle backward
    if (reverse && window->prev_sub_window) {
        return window->prev_sub_window;
    }
    // Wrap forward
    if (!reverse && !window->next_sub_window && wrap) {
        DebugOut(L"wrap from %s %s\n", window->exe_name.text, window->title.text);
        while (window->prev_sub_window) { window = window->prev_sub_window; }
    }
    // Wrap backward
    if (reverse && !window->prev_sub_window && wrap) {
        DebugOut(L"wrap from %s %s\n", window->exe_name.text, window->title.text);
        while (window->next_sub_window) { window = window->next_sub_window; }
    }
    // Can be NULL?
    return window;
}

/*
void enum_windows() {
    windows_count = 0; // global var
    //DebugOut(L"EnumWindows (all windows) (+ means Alt-Tab window, - means not Alt-Tab window)\n");
    EnumWindows(addwindow, NULL);
    //EnumWindows(enum_windows_proc, &windows_count);
    //DebugOut(L"%lld Alt-Tab windows\n", windows_count);
    linkwindows();

    // map unique indices
    char *data[20];
    int i, j, n, unique[20];

    n = 0;
    for (i = 0; i<20; ++i) {
        for (j = 0; j<n; ++j) {
            if (!strcmp(data[i], data[unique[j]]))
                break;
        }

        if (j==n)
            unique[n++] = i;
    }

}
void next_window(HWND hwnd, int offset) {
    linked_window_t next;
    for (int i = 0; i<windows_count; i++) {
        if (windows[i].window == hwnd) {
            next = windows[i];
            break;
        }
    }
    while (offset--) {
        next = *next.next_sub_window;
    }
    DebugOut(L"%s\n", title(next.window).text);
}
void next_top_window(HWND hwnd, int offset) {
    linked_window_t *next = NULL;
    for (int i = 0; i<windows_count; i++) {
        if (windows[i].window==hwnd) {
            next = &windows[i];
            break;
        }
    }
    while (offset--) {
        next = next->next_top_window;
    }
    DebugOut(L"%s\n", title(next->hwnd).text);
}

// https://stackoverflow.com/a/52488331/659310
void enumWindows(WNDENUMPROC in_Proc, LPARAM in_Param) {

    // get desktop window, this is equivalent to specifying NULL as hwndParent
    void *desktop_window = GetDesktopWindow();

    // fallback to using FindWindowEx, get first top-level window
    void *first_window = FindWindowExW(desktop_window, 0, 0, 0);

    // init the enumeration
    int count = 0;
    int result = true;
    void *current_window = first_window;

    // loop through windows found
    // - since 2012 the EnumWindows API in windows has a problem (on purpose by MS)
    //   that it does not return all windows (no metro apps, no start menu etc)
    // - luckally the FindWindowEx() still is clean and working
    while (current_window && result) {
        // call the callback
        if (in_Proc!=NULL) {
            result = in_Proc(current_window, in_Param);
        }

        // get next window
        current_window = FindWindowExW(desktop_window, current_window, 0, 0);

        // protect against changes in window hierachy during enumeration
        if (current_window == first_window || count++ > 10000)
            break;
    }

    DebugOut(L"%i\n", count);
}
*/


// rename next_top_window        -> next_top_window
// rename next_sub_window -> next_sub_window
linked_window_t *top(linked_window_t *window) {
    return (window->prev_sub_window == NULL);
}
linked_window_t *bottom(linked_window_t *window) {

}
linked_window_t *first(linked_window_t *window) {
    if (top(window)) {
        while (window->prev_top_window) { window = window->prev_top_window; }
        return window;
    } else {
        while (window->prev_sub_window) { window = window->prev_sub_window; }
        return window;
    }
}
linked_window_t *root(linked_window_t *window) {
    if (top(window)) {
        return first(window);
    } else {
        return first(first(window));
    }
}
linked_window_t *last(linked_window_t *window) {
    if (top(window)) {
        while (window->next_top_window) { window = window->next_top_window; }
        return window;
    } else {
        while (window->next_sub_window) { window = window->next_sub_window; }
        return window;
    }
}
linked_window_t *next(linked_window_t *window, bool reverse, bool wrap) {
    if (top(window)) {



    } else {

    }
}


#define WM_USER_TRAYICON WM_USER + 1

intptr_t CALLBACK WndProc(void *hWnd, unsigned int msg, intptr_t wParam, intptr_t lParam) {
    switch (msg) {
        case WM_CREATE:
        {

        }
        break;
        case WM_HOTKEY:
        {
            //DebugOut(L"Here's a pointer for you: %" PRIxPTR "\n", wParam);
            DebugOut(L"Here: %d\n", wParam);
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
            //DebugOut(L"Foreground: %s\n", title);
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



intptr_t CALLBACK LowLevelKeyboardProc(int nCode, intptr_t wParam, intptr_t lParam) {
    if (nCode != HC_ACTION)
        return;
    //
    // pressed_keys
    // 
    // Char-based bitarray
    // usage:
    //   char bitarray[BITCOUNT(47)];
    //   BITSET(bitarray, 23);
    //   if (BITTEST(bitarray, 35))
#include <limits.h>	   /* for CHAR_BIT */
#define BITCOUNT(n)    ((n + CHAR_BIT - 1) / CHAR_BIT)
#define BITINDEX(b)    ((b) / CHAR_BIT)
#define BITMASK(b)     (1 << ((b) % CHAR_BIT))
#define BITSET(a, b)   ((a)[BITINDEX(b)] |=  BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITINDEX(b)] &= ~BITMASK(b))
#define BITTEST(a, b)  ((a)[BITINDEX(b)] &   BITMASK(b))
//
// Manually track modifier state and key repeat with a bit array (key repeat must be tracked manually in low level hook)
// vkCode: "The code must be a value in the range 1 to 254." (docs)
// 256 bits for complete tracking of currently held virtual keys (vkCodes, i.e. VK_* defines).
    static char pressed_keys[BITCOUNT(256)] = {0};

    // Window selection, incremented/(decremented) on (Shift+)Alt+Tab & (Shift+)Alt+` press & repeat, reset on Alt+Esc & Alt keyup, see below
    static linked_window_t *selected_app = NULL;
    static linked_window_t *selected_app_window = NULL;
    static linked_window_t *selected_app_restore = NULL;

    // Info about the keyboard for the low level hook
    KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;

    // vkCode technically fits in a 'char' (see above), but we'll use the same type as the struct
    unsigned long virtual_key = kb->vkCode;
    unsigned long scan_code = kb->scanCode;

    // Manually track key repeat using 'pressed_keys' as a bit array (see above)
    int key_was_down = BITTEST(pressed_keys, virtual_key);
    int key_down = !(kb->flags & LLKHF_UP);
    int key_repeat = key_was_down && key_down;

    if (!key_down) {
        BITCLEAR(pressed_keys, virtual_key);
    }
    if (key_down && !key_repeat) {
        BITSET(pressed_keys, virtual_key);
    }

    //DebugOut(L"virtual_key:%llu\tscanCode:%llu\tdown:%d\trepeat:%d\n", virtual_key, scanCode, bKeyDown, bKeyRepeat);

    // ShiftKey & AltKey: Note the shift & alt keys are the *left* keys (VK_LSHIFT & VK_LMENU)
    // Key29: Scan code 0x29 (hex) = 41 (decimal). It's the key that is physically below Esc, left of 1 and above Tab
    int shift_held = BITTEST(pressed_keys, VK_LSHIFT);
    int alt_held = BITTEST(pressed_keys, VK_LMENU);
    int alt_up = virtual_key == VK_LMENU && !key_down;
    int tab_down = virtual_key == VK_TAB && key_down && !key_repeat;
    int tab_repeat = virtual_key == VK_TAB && key_down && key_repeat;
    int key29_down = scan_code == 41 && key_down && !key_repeat;
    int key29_repeat = scan_code == 41 && key_down && key_repeat;
    int esc_down = virtual_key == VK_ESCAPE && key_down;

    // Note:
    // By returning a non-zero value from the hook procedure, the message is consumed and does not get passed to the target window

    // Alt+Tab
    if (alt_held && tab_down) {
        if (selected_app == NULL && selected_app_window == NULL) {
            // We've been reset and are starting from scratch so get all windows
            linkwindows();
            print_windows(false);
            DebugOut(L"foreground_app: %s\n", title(GetForegroundWindow()).text);
            // TODO testing fast_switching
            selected_app_restore = linked_window(GetForegroundWindow());
        }

        linked_window_t *next = next_top_window(selected_app, shift_held, true); // yes wrap-around

        if (selected_app != next && next) {
            selected_app = next;
            DebugOut(L"selected_app: %s %s\n", selected_app->exe_name.text, selected_app->title.text);
        }

        if (!next) {
            DebugOut(L"whoops! next selected app is NULL!\n");
        }

        // TODO Testing "immediate" / "fast" mode:
        bool fast_switching = true;
        if (selected_app && fast_switching) {
            SetForegroundWindow(selected_app->hwnd);
            return 1;
        }

        // Show window
        int windowGotShown = !ShowWindow(hMainWnd, SW_SHOW);
        if (windowGotShown) {
            UpdateWindow(hMainWnd); // TODO / BUG: Is this necessary? It's here since the template code included it when showing window
        }

        // Consume message
        return 1;
    }
    // Alt+Tab repeat
    if (alt_held && tab_repeat) {

        linked_window_t *next = next_top_window(selected_app, shift_held, false); // no wrap-around

        if (selected_app != next && next) {
            selected_app = next;
            DebugOut(L"selected_app repeat: %s %s\n", selected_app->exe_name.text, selected_app->title.text);
        }

        if (!next) {
            DebugOut(L"whoops! next selected app is NULL!\n");
        }

        // Consume message
        return 1;
    }
    // Alt+`
    if (alt_held && key29_down) {
        if (selected_app == NULL && selected_app_window == NULL) {
            // We've been reset and are starting from scratch so get all windows
            linkwindows();
            //print_windows(false);
            //DebugOut(L"foreground_app: %s\n", title(GetForegroundWindow()).text);
        }

        linked_window_t *next = next_sub_window(selected_app_window, shift_held, true); // yes wrap-around

        if (selected_app_window != next && next) {
            selected_app_window = next;
            DebugOut(L"selected_app_window: %s %s\n", selected_app_window->exe_name.text, selected_app_window->title.text);
        }

        if (!next) {
            DebugOut(L"whoops! next selected app window is NULL!\n");
        }

        if (selected_app_window) {
            SetForegroundWindow(selected_app_window->hwnd);
        }

        // Consume message
        return 1;
    }
    // Alt+` repeat
    if (alt_held && key29_repeat) {
        linked_window_t *next = next_sub_window(selected_app_window, shift_held, false); // no wrap-around

        if (selected_app_window != next && next) {
            selected_app_window = next;
            DebugOut(L"selected_app_window repeat: %s %s\n", selected_app_window->exe_name.text, selected_app_window->title.text);
        }

        if (!next) {
            DebugOut(L"whoops! next selected app window is NULL!\n");
        }

        if (selected_app_window) {
            SetForegroundWindow(selected_app_window->hwnd);
        }

        // Consume message
        return 1;
    }
    // Alt keyup
    if (alt_up) {
        if (selected_app) {
            SetForegroundWindow(selected_app->hwnd);
        }

        // Reset selection
        selected_app = NULL;
        selected_app_window = NULL;

        // Hide window
        int windowGotHidden = ShowWindow(hMainWnd, SW_HIDE);
        if (windowGotHidden) {
            DebugOut(L"hide: alt keyup\n");
            //return 1; // Consume message BUG: Alt key gets stuck if we consume this message, but this also seems to propogate an Alt keydown event to the underlying window?
        } else {
            // The hook caught Alt keyup, but the switcher window was already hidden
            // Since this is a no-op for us, we pass the message on
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }
    }
    // Alt+Esc
    if (alt_held && esc_down) {
        // Reset selection
        selected_app = NULL;
        selected_app_window = NULL;

        // TODO testing fast_switching
        if (selected_app_restore) { /* && fast_switching */
            SetForegroundWindow(selected_app_restore->hwnd);
            selected_app_restore = NULL;
            return 1;
        }

        // Hide window
        int windowGotHidden = ShowWindow(hMainWnd, SW_HIDE);
        if (windowGotHidden) {
            DebugOut(L"hide: alt+esc\n");
            return 1; // Consume message
        } else {
            // The hook caught Alt+Esc, but the switcher window was already hidden
            // Since this is a no-op for us, we pass the message on
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}



// Message handler for about box.
intptr_t CALLBACK About(void *hWnd, unsigned int msg, intptr_t wParam, intptr_t lParam) {
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
