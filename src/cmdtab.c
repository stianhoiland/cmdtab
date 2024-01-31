#define _UNICODE
#define UNICODE

#ifndef UNICODE
#include <stdio.h>
#endif

#include <windows.h>
#include <tchar.h>

int _tmain(int argc, TCHAR** argv)
{
    PDWORD cChars = NULL;
    HANDLE std = GetStdHandle(STD_OUTPUT_HANDLE);

    if (std == INVALID_HANDLE_VALUE) {
        _tprintf(_T("Cannon retrieve standard output handle\n (%d)"), GetLastError());
    }

    if (argv[1]) {
        WriteConsole(std, argv[1], _tcslen(argv[1]), cChars, NULL);
    }

    CloseHandle(std);

    return 0;
}





#include "targetver.h"


// Windows Header Files
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// C Host Header Files
#include <stdlib.h>
#include <malloc.h>
//#include <memory.h>
#include <tchar.h>

#include "cmdtab3.h"


// Freestanding C
// C89
#include <float.h>
#include <limits.h>
#include <iso646.h>
#include <stdarg.h>
#include <stddef.h>
// C99
#include <stdbool.h>
#include <stdint.h>
// C11
#include <stdalign.h>
#include <stdnoreturn.h>






#pragma comment(lib,"user32.lib") 

#include <stdbool.h>
#include <stdint.h>
#include <uchar.h>

typedef int8_t     i2; // -1 & 0, error & success? (wouldn't used a smaller type if there was, i.e. _BitInt())
typedef int8_t     i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;

typedef uint8_t    u2; // 0 && 1, false & true?
typedef uint8_t    u8; 
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef float     f32;
typedef double    f64;

typedef char16_t  c16;
typedef int32_t   b32;
typedef char      byte;
typedef ptrdiff_t size;
typedef size_t    usize;

typedef uintptr_t uptr; // unsigned
typedef intptr_t  iptr; // signed

typedef void zero;
typedef void u0;
typedef void i0;
typedef void u1; // sizeof(void) is actually 1
typedef void i1; // sizeof(void) is actually 1






#define W32(r) __declspec(dllimport) r __stdcall
W32(void) ExitProcess(u32);
W32(i32) GetStdHandle(u32);
W32(byte*) VirtualAlloc(byte*, usize, u32, u32);
W32(b32) WriteConsoleA(uptr, u8*, u32, u32*, void*);
W32(b32) WriteConsoleW(uptr, c16*, u32, u32*, void*);


EnumWindows
GetForegroundWindow
GetWindowTextW
GetWindowThreadProcessId
SetForegroundWindow
RegisterHotKey


// cmdtab3.cpp : Defines the entry point for the application.
//
















#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name




#define W32(r) __declspec(dllimport) r __stdcall
W32(void*) GetStdHandle(int);
W32(int) WriteFile(void*, void*, int, int*, void*);

int mainCRTStartup(void)
{
    void* stdout = GetStdHandle(-10 - 1);
    char message[] = "Hello, world!\n";
    int len;
    return !WriteFile(stdout, message, sizeof(message) - 1, &len, 0);
}



void DebugOut(wchar_t* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    wchar_t dbg_out[4096];
    vswprintf_s(dbg_out, 4096, fmt, argp);
    va_end(argp);
    OutputDebugString(dbg_out);
}



BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    DWORD id = GetWindowThreadProcessId(hwnd, NULL);
    DebugOut(_T("%lu\n"), id);
}




int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow)
{
    printf("Hello world!\n");
    fflush(stdout);
    _tprintf(_T("Not working\n"));

    DebugOut(_T("Start\n"));
    RegisterHotKey(NULL, 1, MOD_ALT | MOD_NOREPEAT, 0xDC); // alt + VK_OEM_5
    EnumWindows(EnumWindowsProc, NULL);

    MSG msg = { 0 };
    while (GetMessage(&msg, NULL, 0, 0) != 0)
    {
        if (msg.message == WM_HOTKEY)
        {
            OutputDebugString(_T("WM_HOTKEY received\n"));
            //EnumWindows(EnumWindowsProc, NULL);
            return TRUE;
        }
    }

    return TRUE;
    /*
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CMDTAB3, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CMDTAB3));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
    */
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CMDTAB3));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CMDTAB3);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
