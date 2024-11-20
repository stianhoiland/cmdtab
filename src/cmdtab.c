#define WINDOWS_LEAN_AND_MEAN
#define COBJMACROS
#define _CRTDBG_MAP_ALLOC

#include <stdlib.h>
//#include <stdbool.h>
#include <string.h>

#include <stdarg.h>    // Used by print
#include <limits.h>	   // for CHAR_BIT
//#include <stddef.h>
#include <math.h>

#include <crtdbg.h>
#include <windows.h>
//#include <minwindef.h>
//#include <minwinbase.h>

//#include <malloc.h>
//#include <memory.h>

//#include <wchar.h>     // We're wiiiide, baby!

#pragma comment(lib, "user32.lib")
#include "winuser.h"   // Used by gettitle, getpath, filter, link, next
#pragma comment(lib, "shlwapi.lib") 
#include "shlwapi.h"   // Used by getname
#pragma comment(lib, "dwmapi.lib") 
#include "dwmapi.h"    // Used by filter

#include "processthreadsapi.h" // OpenProcess, kernel32.lib
#include "winbase.h"           // QueryFullProcessImageNameW, kernel32.lib
#include "handleapi.h"         // CloseHandler, kernel32.lib

#pragma comment(lib, "pathcch.lib")
#include "pathcch.h"   // Used by getfilename

#include "strsafe.h"   // Used by getappname

#include "commoncontrols.h" // Used by geticon, needs COBJMACROS, ole32.lib

#pragma comment(lib, "version.lib") // Used by appname
#pragma comment(lib, "winmm.lib") // For PlaySound, TODO Remove

#pragma comment(lib, "uxtheme.lib") // TODO Remove
#include <uxtheme.h> // TODO Remove

//==============================================================================
// Linker directives
//==============================================================================

#if defined _WIN64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

//==============================================================================
// Utility macros
//==============================================================================

#define debug() __debugbreak() // Or DebugBreak in debugapi.h
#define countof(a) (ptrdiff_t)(sizeof(a) / sizeof(*a))

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
#define bitindex(b)   ((b) / CHAR_BIT)
#define bitmask(b)    (1 << ((b) % CHAR_BIT))
#define setbit1(a, b) ((a)[bitindex(b)] |=  bitmask(b))
#define setbit0(a, b) ((a)[bitindex(b)] &= ~bitmask(b))
#define getbit(a, b)  ((a)[bitindex(b)] &   bitmask(b))

#define false 0
#define true 1

typedef unsigned char        u8;
typedef wchar_t             u16; // "Microsoft implements wchar_t as a two-byte unsigned value."
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed short        i16;
typedef signed int          i32;
typedef signed long long    i64;
typedef signed int         bool;
typedef ptrdiff_t          size;
typedef size_t               uz;
//typedef struct {int _;} *handle;
typedef void *           handle;
typedef struct tagRECT     rect;
typedef unsigned int     vkcode;

//#ifndef MAX_PATH
//#define MAX_PATH 260
//#endif

/*
#define W32(r)  __declspec(dllimport) r __stdcall
W32 (bool)   CloseHandle(handle);
W32 (handle) CreateFileW(c16 *, u32, u32, void *, u32, u32, handle);
W32 (void)   ExitProcess(u32) __attribute((noreturn));
*/
/*
#define W32(r, p) extern "C" __declspec(dllimport) r __stdcall p noexcept
W32(void,   ExitProcess[[noreturn]](i32));
W32(uz,     GetStdHandle(i32));
W32(i32,    ReadFile(uz, u8 *, i32, i32 *, uz));
W32(byte *, VirtualAlloc(uz, iz, i32, i32));
W32(i32,    WriteFile(uz, u8 *, i32, i32 *, uz));
*/



typedef struct string string;
typedef struct app app_t;
typedef struct gdi gdi_t;
typedef struct identifier identifier;
typedef struct config config_t;

#define string(str) { str, countof(str)-1, true } // TODO: -1 to remove NUL-terminator?

struct string {
	u16 text[(MAX_PATH*2)+1];
	u32 length;
	bool ok;
};

// struct txt {
//	 u16 len;
//	 u16 *data;
// };
// txtcmp
// txteq
// txtcpy
// txtdup?
// txtcat
// txtchr, txtrchr
// txtstr, txtrstr

struct app {
	string filepath; // aka. full module path
	string appname; // aka. product name
	handle icon;
	handle hwnds[64]; // Filtered windows that can be switched to
	size hwnds_count; // Number of elements in 'hwnds' array
};

struct gdi {
	handle context;
	handle bitmap;
	rect rect;
	//HBRUSH bg;
};

struct config {
	// Hotkeys
	vkcode mod1;
	vkcode key1;
	vkcode mod2;
	vkcode key2;
	// Behavior
	bool switch_apps;
	bool switch_windows;
	bool fast_switching_apps; // BUG / TODO Not ideal. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
	bool fast_switching_windows; // BUG / TODO Not ideal. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
	bool show_gui_for_apps;
	bool show_gui_for_windows;
	u32  show_gui_delay;
	bool wrap_bump;
	bool ignore_minimized; // TODO? Atm, 'ignore_minimized' affects both Alt-Tilde/Backquote and Alt-Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.) TODO Should this be called 'restore_minimized'?
	bool restore_on_cancel;
	// Appearance
	u32 gui_height;
	u32 gui_horz_margin;
	u32 gui_vert_margin;
	u32 icon_width;
	u32 icon_horz_padding;
	// Blacklist
	struct identifier {
		u16 *filename; // File name without file extension, ex. "explorer"
		u16 *class;    // Window class name
	} blacklist[32];
};

static string UWPHostExecutable = string(L"ApplicationFrameHost.exe");
static string UWPHostClass      = string(L"ApplicationFrameWindow");
static string UWPHostedClass    = string(L"Windows.UI.Core.CoreWindow");

//==============================================================================
// App state
//==============================================================================

// Go from supporting max. 256 windows @ 690kb to max. 8192 windows @ 340kb (49% memory decrease, 3200% capacity increase),
//  but sectioned into max. 128 apps, each with max. 64 windows

static app_t apps[128];            // Filtered windows that are displayed in switcher
static size apps_count;            // Number of elements in 'apps' array
static app_t *selected_app;        
static handle *selected_window;    // Currently selected window in switcher. Non-NULL indicates switcher is active
static handle foregrounded_window; // Last switched-to window
static handle restore_window;      // Whatever window was switched away from. Does not need to be in filtered 'apps' array
static handle gui;                 // Handle for main window (switcher)
static gdi_t gdi;                  // Off-screen bitmap used for double-buffering
static u8 keyboard[32];            // 256 bits for tracking key repeat when using low-level keyboard hook
static i32 mouse_x, mouse_y;

static void _showgui(handle, u32, u64, u32);   // TimerProc used by showgui
static bool _addwindow(handle, i64);           // EnumWindowsProc used by addwindow
static bool _getuwp(handle, i64);              // EnumChildProc used by getuwp

static config_t config = {
	
	// The default key2 is scan code 0x29 (hex) / 41 (decimal)
	// This is the key that is physically below Esc, left of 1 and above Tab
	//
	// WARNING!
	// VK_OEM_3 IS NOT SCAN CODE 0x29 FOR ALL KEYBOARD LAYOUTS.
	// YOU MUST CALL THE FOLLOWING DURING RUNTIME INITIALIZATION:
	// `.key2 = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX)`
	
	// Hotkeys
	.mod1 = VK_LMENU,
	.key1 = VK_TAB,
	.mod2 = VK_LMENU,
	.key2 = VK_OEM_3, // Must be updated during runtime. See note above.
	// Behavior
	.switch_apps            = true,
	.switch_windows         = true,
	.fast_switching_apps    = false, // default false
	.fast_switching_windows = true,
	.show_gui_for_apps      = true,
	.show_gui_for_windows   = false,
	.show_gui_delay         = 0,
	.wrap_bump              = true,
	.ignore_minimized       = false,
	.restore_on_cancel      = false,
	// Appearance
	.gui_height        = 128,
	.icon_width        =  64,
	.icon_horz_padding =   8,
	.gui_horz_margin   =  24,
	.gui_vert_margin   =  32,
	// Blacklist
	.blacklist = {
		//{ NULL,                    L"Windows.UI.Core.CoreWindow" },
		//{ NULL,                    L"Xaml_WindowedPopupClass" },
		{ NULL,                      L"ApplicationFrameWindow" },
		//{ NULL ,                   L"ApplicationFrameHost" },
		//{ L"explorer",             L"ApplicationFrameWindow" },
		//{ L"explorer",             L"ApplicationManager_DesktopShellWindow" },
		//{ L"ApplicationFrameHost", L"ApplicationFrameWindow" },
		//{ L"TextInputHost",        NULL },
	},
};

//==============================================================================
// Here we go!
//==============================================================================

static void print(u16 *fmt, ...)
{
#ifdef _DEBUG
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	OutputDebugStringW(buffer);
#endif
}
static void copy(string *src, string *dst)
{
	
}
static bool equals(string *s1, string *s2)
{
	// Reverse compare strings
	// Compare in reverse since this function is mostly used to compare file paths, which are often unique yet have long identical prefixes
	if (s1->length != s2->length) {
		return false;
	}
	for (int i = s1->length - 1; i >= 0; i--) {
		if (s1->text[i] != s2->text[i]) {
			return false;
		}
	}
	return true;
}
static bool endswith(string *s1, string *s2) 
{
	u16 *str = s1->text;
	u16 *fix = s2->text;
	size str_len = s1->length;
	size fix_len = s2->length;
	return str_len >= fix_len && !memcmp(str+str_len-fix_len, fix, fix_len);
}

static string getclass(handle hwnd)
{
	string class = {0};
	class.length = GetClassNameW(hwnd, class.text, countof(class.text));
	class.ok = (class.length > 0);
	return class;
}
static handle getuwp(handle hwnd)
{
	string class = getclass(hwnd);
	if (!equals(&class, &UWPHostClass)) {
		return hwnd;
	}
	handle hwnd2 = hwnd;
	EnumChildWindows(hwnd, _getuwp, &hwnd2); // _getuwp returns hosted window, if any
	return hwnd2;
}
static bool _getuwp(handle hwnd, i64 lparam)
{
	// EnumChildProc
	// Set inout param to child window of UWP host if child window has hosted class
	string class = getclass(hwnd);
	if (equals(&class, &UWPHostedClass)) {
		*(handle *)lparam = hwnd;
		return false; // Found the hosted window, so we're done
	} else {
		return true; // Return true to continue EnumChildWindows
	}
}
static string getfilepath(handle hwnd)
{
	// How to get the filepath of the executable for the process that spawned the window:
	// 1. GetWindowThreadProcessId() to get process id for window
	// 2. OpenProcess() for access to remote process memory
	// 3. QueryFullProcessImageNameW() [Windows Vista] to get full "executable image" name (i.e. exe path) for process
	
	string path = {0};

	u32 pid;
	GetWindowThreadProcessId(hwnd, &pid);
	handle process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid); // NOTE See GetProcessHandleFromHwnd()
	if (!process || process == INVALID_HANDLE_VALUE) {
		print(L"ERROR couldn't open process with pid: %u\n", pid);
		return path;
	}
	path.length = countof(path.text);
	path.ok = QueryFullProcessImageNameW(process, 0, path.text, &path.length); // NOTE Can use GetLongPathName() if getting shortened paths
	CloseHandle(process);
	if (!path.ok) {
		print(L"ERROR couldn't get exe path for process with pid: %u\n", pid);
		return path;
	}
	return path;
}
static string gettitle(handle hwnd)
{
	string title = {0};
	title.length = GetWindowTextW(hwnd, title.text, countof(title.text));
	title.ok = (title.length > 0);
	return title;
}
static string getfilename(u16 *filepath, bool extension)
{
	// Just yer normal filepath string manipulation. Aka. trimpath?
	string name = {0};
	name.ok = SUCCEEDED(StringCchCopyNW(name.text, countof(name.text), PathFindFileNameW(filepath), countof(name.text)));
	if (!extension) {
		name.ok = name.ok && SUCCEEDED(PathCchRemoveExtension(name.text, countof(name.text)));
	}
	name.ok = name.ok && SUCCEEDED(StringCchLengthW(name.text, countof(name.text), &name.length));
	return name;
}
static string getappname(u16 *filepath)
{
	// Get pretty application name
	//
	// Don't even ask me bro why it has to be like this
	// Thanks to 'Aric Stewart' for an ok-ish looking version of this that I
	// adapted a little
	//
	// For a possible refactor, see https://stackoverflow.com/a/51950848/659310
	// and SHCreateItemFromParsingName.

	// Our return value:
	string name = {0};

	u32 size, handle;
	void *version;
	struct {
		u16 language;
		u16 codepage;
	} *translation;
	u32 length;
	u16 buffer[MAX_PATH];
	u16 *result;

	if (!(size = GetFileVersionInfoSizeW(filepath, &handle))) {
		goto abort;
	}
	if (!(version = malloc(size))) {
		goto abort;
	}
	if (!GetFileVersionInfoW(filepath, handle, size, version)) {
		goto free;
	}
	if (!VerQueryValueW(version, L"\\VarFileInfo\\Translation", (void *)&translation, &length)) {
		goto free;
	}

	StringCchPrintfW(buffer, countof(buffer), L"\\StringFileInfo\\%04x%04x\\FileDescription", translation[0].language, translation[0].codepage);
	
	if (!VerQueryValueW(version, buffer, (void *)&result, &length)) {
		goto free;
	}
	if (!length || !result) {
		goto free;
	}

	if (length == 1) {
		// Fall back to 'ProductName' if 'FileDescription' is empty
		// Example: Github Desktop
		StringCchPrintfW(buffer, countof(buffer), L"\\StringFileInfo\\%04x%04x\\ProductName", translation[0].language, translation[0].codepage);
		
		if (!VerQueryValueW(version, buffer, (void *)&result, &length)) {
			goto free;
		}
		if (!length || !result) {
			goto free;
		}
	}

	name.length = length+1; // +1 fixes #2 ¯\_(ツ)_/¯
	name.ok = SUCCEEDED(StringCchCopyNW(name.text, countof(name.text), result, countof(name.text)));

	free:
		free(version);
	abort:
		return name;
}
static handle geticon(u16 *filepath) 
{
	// From Raymond Chen: https://stackoverflow.com/a/12429114/659310
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
	// NOTE: Can also check out SHDefExtractIcon()
	//       https://devblogs.microsoft.com/oldnewthing/20140501-00/?p=1103

	if (FAILED(CoInitialize(NULL))) { // In case we use COM
		return NULL;
	}
	SHFILEINFOW info = {0};
	if (!SHGetFileInfoW(filepath, 0, &info, sizeof info, SHGFI_SYSICONINDEX)) { // 2nd arg: -1, 0 or FILE_ATTRIBUTE_NORMAL
		debug(); // SHGetFileInfoW() has special return with SHGFI_SYSICONINDEX flag, which I think should never fail (docs unclear?)
		return NULL;
	}
	IImageList *list = {0};
	if (FAILED(SHGetImageList(SHIL_JUMBO, &IID_IImageList, (void **)&list))) {
		return NULL;
	}
	HICON icon = {0};
	//int w, h;
	IImageList_GetIcon(list, info.iIcon, ILD_TRANSPARENT, &icon); //ILD_IMAGE
	//IImageList_GetIconSize(list, &w, &h);
	IImageList_Release(list);
	return icon; // Caller is responsible for calling DestroyIcon 
}

static bool getkey(vkcode key)
{
	return getbit(keyboard, key);
}
static bool setkey(vkcode key, bool down)
{
	bool wasdown = getkey(key);
	if (down) {
		setbit1(keyboard, key);
	} else {
		setbit0(keyboard, key);
	}
	return wasdown && down; // Return true if this was a key repeat
}
static void synckeys()
{
	for (int i = 0; i < sizeof keyboard * CHAR_BIT; i++) {
		if (GetAsyncKeyState(i) & 0x8000) {
			setbit1(keyboard, i);
		} else {
			setbit0(keyboard, i);
		}
	}
}

static void printhwnd(handle hwnd)
{
	if (hwnd) {
		string filepath = getfilepath(hwnd);
		if (!filepath.ok) return;
		string filename = getfilename(filepath.text, true);
		string class = getclass(hwnd);
		string title = gettitle(hwnd);
		u16 *ws_iconic           = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_ICONIC) ? L" (minimized)" : L"";
		u16 *ws_visible          = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_VISIBLE) ? L"visible" : L"not visible";
		u16 *ws_child            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) ? L", child" : L", parent";
		u16 *ws_disabled         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DISABLED) ? L", disabled" : L", enabled";
		u16 *ws_popup            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUP) ? L", popup" : L"";
		u16 *ws_popupwindow      = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUPWINDOW) ? L", popupwindow" : L"";
		u16 *ws_tiled            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_TILED) ? L", tiled" : L"";
		u16 *ws_tiledwindow      = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_TILEDWINDOW) ? L", tiledwindow" : L"";
		u16 *ws_overlapped       = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPED) ? L", overlapped" : L"";
		u16 *ws_overlappedwindow = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW) ? L", overlappedwindow" : L"";
		u16 *ws_dlgframe         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DLGFRAME) ? L", dlgframe" : L"";
		u16 *ex_toolwindow       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ? L", toolwindow" : L"";
		u16 *ex_appwindow        = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) ? L", appwindow" : L"";
		u16 *ex_dlgmodalframe    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_DLGMODALFRAME) ? L", dlgmodalframe" : L"";
		u16 *ex_layered          = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) ? L", layered" : L"";
		u16 *ex_noactivate       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) ? L", noactivate" : L"";
		u16 *ex_palettewindow    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_PALETTEWINDOW) ? L", palettewindow" : L"";
		u16 *ex_overlappedwindow = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_OVERLAPPEDWINDOW) ? L", exoverlappedwindow" : L"";
		print(L"%s%s %s \"%.32s\"", filename.text, ws_iconic, class.text, title.text);
		print(L" (%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s)\n",
			ws_visible, ws_child, ws_disabled, ws_popup, ws_popupwindow, ws_tiled, ws_tiledwindow, ws_overlapped, ws_overlappedwindow, ws_dlgframe,
			ex_toolwindow, ex_appwindow, ex_dlgmodalframe, ex_layered, ex_noactivate, ex_palettewindow, ex_overlappedwindow);
	} else {
		print(L"NULL hwnd\n");
	}
}
static void printapps() {
	for (int i = 0; i < apps_count; i++) {
		print(L"%i  ", i);
		app_t *app = &apps[i];
		printhwnd(app->hwnds[0]);
		for (int j = 1; j < app->hwnds_count; j++) {
			print(L"%i    ", j);
			handle *window = &app->hwnds[i];
			printhwnd(*window);
		}
	}
	print(L"\n");
}

static void close(handle hwnd)
{
	//static const int WM_CLOSE = 0x0010;
	SendMessageW(hwnd, WM_CLOSE, 0, 0);
}
static void minimize(handle hwnd)
{	
	//static const int WM_SYSCOMMAND = 0x0112;
	//static const int SC_MINIMIZE = 0xF020;
	SendMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}
static void preview(handle hwnd)
{
	typedef HRESULT (__stdcall *DwmpActivateLivePreview)(bool peekOn, handle hPeekWindow, handle hTopmostWindow, u32 peekType1or3, i64 newForWin10);
	//DwmpActivateLivePreview(true, hwnd, gui, 3, 0);
}
static void hack()
{
	// Oh my, the fucking rabbit hole...
	// This is a hack (one of many flavors) to bypass the restrictions on setting the foreground window
	// https://github.com/microsoft/PowerToys/blob/7d0304fd06939d9f552e75be9c830db22f8ff9e2/src/modules/fancyzones/FancyZonesLib/util.cpp#L376
	// gg windows ¯\_(ツ)_/¯
	SendInput(1, &(INPUT){.type = INPUT_KEYBOARD}, sizeof(INPUT));
}
static void altup()
{
	//SendInput(1, &(INPUT){.type = INPUT_KEYBOARD, .ki.wVk = VK_MENU, .ki.dwFlags = KEYEVENTF_KEYUP}, sizeof(INPUT));
}
static void show(handle hwnd)
{
	if (IsIconic(hwnd)) {
		ShowWindow(hwnd, SW_RESTORE);
	} else {
		ShowWindow(hwnd, SW_SHOWNA);
	}
	if (GetForegroundWindow() != hwnd) {
		hack();
		SetForegroundWindow(hwnd);
	} else {
		print(L"already foreground\n");
	}
}
static void showgui(u32 delay)
{
	KillTimer(gui, /*timerid*/ 1);
	if (delay > 0) {
		SetTimer(gui, /*timerid*/ 1, delay, _showgui); // _showgui calls showgui with 0 delay
		return;
	}
	ShowWindow(gui, SW_SHOWNA);
	//LockSetForegroundWindow
}
static void _showgui(handle hwnd, u32 msg, u64 eventid, u32 time)
{
	// TimerProc
	// Call showgui with 0 delay when timer activates
	showgui(0);
}

static bool ask(u16 *fmt, ...)
{
	//static const int IDOK = 1;
	//static const int IDYES = 6;
	//static const int MB_YESNO = 0x00000004L; 
	//static const int MB_ICONQUESTION  0x00000020L;
	//static const int MB_TASKMODAL = 0x00002000L;
	
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);

	int result = MessageBoxW(gui, buffer, L"cmdtab", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL);
	return result == IDOK || result == IDYES;
}
static void snitch(handle hwnd)
{
	// Tell user the executable name and class so they can add it to the blacklist
	string class = getclass(hwnd);
	string path = getfilepath(hwnd);
	string name = getfilename(path.text, false);

	u16 buffer[2048];
	StringCchPrintfW(buffer, countof(buffer)-1, L"{%s, %s}\n\nAdd to blacklist (NOT WORKNG YET, TODO!)?", name.text, class.text);

	int result = MessageBoxW(gui, buffer, L"cmdtab", MB_YESNO | MB_ICONASTERISK | MB_TASKMODAL);
	return result == IDOK || result == IDYES;
}

static bool ignored(handle hwnd)
{
	// Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
	if (IsWindowVisible(hwnd) == false) {
		return true;
	}
	int cloaked = 0;
	DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked); // Windows 8
	if (cloaked) {
		return true;
	}
	// "A tool window [i.e. WS_EX_TOOLWINDOW] does not appear in the taskbar or
	// in the dialog that appears when the user presses Alt-TAB." (docs)
	if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
		return true;
	}
	if (config.ignore_minimized && IsIconic(hwnd)) { // TODO Hmm, rework or remove this feature
		return true;
	}
	return false;
}
static bool blacklisted(string *filename, string *class)
{
	#define wcseq(str1, str2) (wcscmp(str1, str2) == 0)
	
	// Iterate blacklist. I allow three kinds of entries in the black list:
	// 1) {name,class} both name and class are specified and both must match
	// 2) {name,NULL}  only name is specified and must match
	// 3) {NULL,class} only class is specified and must match
	for (size i = 0; i < countof(config.blacklist); i++) {
		identifier ignored = config.blacklist[i];
		if (ignored.filename && ignored.class) {
			if (wcseq(ignored.filename, filename->text) &&
				wcseq(ignored.class, class->text)) {
				return true;
			}
		} else if (ignored.filename && !ignored.class && wcseq(ignored.filename, filename->text)) {
			return true;
		} else if (!ignored.filename && ignored.class && wcseq(ignored.class, class->text)) {
			return true;
		} else if (!ignored.filename && !ignored.class) {
			break;
		}
	}
	return false;
}
static void addwindow(handle hwnd)
{
	// 1. Get hosted window in case of UWP host
	// 2. Basic checks for eligibility in Alt-Tab
	// 3. Get module filepath, abort if access denied
	// 4. Check user-defined blacklist
	// 5. Get product name, or fallback to filename
	// 6. Lookup app by module filepath in apps list, create new app entry in apps list if necessary
	// 7. Add window to app's windows list
	if (!(hwnd = getuwp(hwnd))) {
		return;
	}
	if (ignored(hwnd)) {
		return;
	}
	string filepath = getfilepath(hwnd);
	if (!filepath.ok) {
		return;
	}
	string filename = getfilename(filepath.text, false);
	string class = getclass(hwnd);
	if (blacklisted(&filename, &class)) {
		return;
	}
	string appname = getappname(filepath.text);
	if (!appname.ok) {
		appname = filename;
	}
	app_t *app = NULL;
	for (int i = 0; i < apps_count; i++) {
		app_t *existing = &apps[i];
		if (equals(&filepath, &existing->filepath)) {
			app = existing;
			break;
		}
	}
	if (!app) {
		if (apps_count >= countof(apps)) {
			print(L"ERROR reached max. apps\n");
			return;
		}
		app = &apps[apps_count++];
		app->filepath = filepath;
		app->appname = appname;
		app->icon = geticon(filepath.text);
	}
	if (app->hwnds_count >= countof(app->hwnds)) {
		print(L"ERROR reached max. windows for app\n");
		return;
	}
	app->hwnds[app->hwnds_count++] = hwnd;
}
static void activate()
{
	// Rebuild our whole window list
	EnumWindows(_addwindow, 0); // _addwindow calls addwindow with every top-level window enumerated by EnumWindows 
	
	//*dbg*/ print(L"________________________________\n");
	//*dbg*/ printwindows(true);

	// aka. selectforground
	// Start by selecting highest Z-ordered window that passes our filters
	// This is not necessarily the system's foreground window, since that window may not have passed our filters
	restore_window = GetForegroundWindow();
	if (apps_count > 0) {
		selected_app = &apps[0];
		selected_window = &selected_app->hwnds[0];
	}
}
static bool _addwindow(handle hwnd, i64 lparam)
{
	// EnumWindowsProc
	// Call addwindow with every top-level window enumerated by EnumWindows
	addwindow(hwnd);
	return true; // Return true to continue EnumWindows
}
static void cancel(bool restore) 
{
	KillTimer(gui, /*timerid*/ 1);
	ShowWindow(gui, SW_HIDE); // Yes, call ShowWindow() to hide window...

	if (restore) { 
		// TODO Check that the restore window is not the second most recently
		//      activated window (after our own switcher window), so that I
		//      don't needlessly switch to the window that will naturally
		//      become activated once our switcher window hides
		show(restore_window);
	}

	selected_app = NULL;
	selected_window = NULL;
	foregrounded_window = NULL;
	restore_window = NULL;

	for (int i = 0; i < apps_count; i++) {
		//if (apps[i].icon) {
		DestroyIcon(apps[i].icon);
		//}
		apps[i].hwnds_count = 0;
	}
	apps_count = 0;

	print(L"cancel\n");
}
static void selectnext(bool applevel, bool reverse, bool wrap)
{
	if (!selected_app || !selected_window) {
		debug();
	}

	if (applevel) {
		app_t *first_app = &apps[0];
		app_t *last_app = &apps[apps_count-1];
		if (reverse) {
			if (selected_app > first_app) {
				selected_app--;
			} else if (wrap) {
				selected_app = last_app;
			}
		} else {
			if (selected_app < last_app) {
				selected_app++;
			} else if (wrap) {
				selected_app = first_app;
			}
		}
		selected_window = &selected_app->hwnds[0];
	} else {
		app_t *first_window = &selected_app->hwnds[0];
		app_t *last_window = &selected_app->hwnds[selected_app->hwnds_count-1];
		if (reverse) {
			if (selected_window > first_window) {
				selected_window--;
			} else if (wrap) {
				selected_window = last_window;
			}
		} else {
			if (selected_window < last_window) {
				selected_window++;
			} else if (wrap) {
				selected_window = first_window;
			}
		}
	}
	//*dbg*/ printhwnd(*selected_window);
}
static void showselected()
{
	//*dbg*/ printhwnd(*selected_window);
	show(*selected_window);
	foregrounded_window = *selected_window;
}

static void resizegui()
{
	u32   icons_width = apps_count * config.icon_width;
	u32 padding_width = apps_count * config.icon_horz_padding * 2;
	u32  margin_width =          2 * config.gui_horz_margin;

	u32 w = icons_width + padding_width + margin_width;
	u32 h = config.gui_height;
	u32 x = GetSystemMetrics(SM_CXSCREEN) / 2 - (w / 2);
	u32 y = GetSystemMetrics(SM_CYSCREEN) / 2 - (h / 2);

	// Resize off-screen double-buffering bitmap
	rect resized = {x, y, x+w, y+h};
	if (!EqualRect(&gdi.rect, &resized)) {
		handle context = GetDC(gui);
		DeleteObject(gdi.bitmap);
		DeleteDC(gdi.context);
		gdi.rect = resized;
		gdi.context = CreateCompatibleDC(context);
		gdi.bitmap = CreateCompatibleBitmap(context, gdi.rect.right - gdi.rect.left, gdi.rect.bottom - gdi.rect.top);
		handle oldbitmap = (handle)SelectObject(gdi.context, gdi.bitmap);
		DeleteObject(oldbitmap);
		ReleaseDC(gui, context);
	}

	MoveWindow(gui, x, y, w, h, false); // Yes, "MoveWindow" means "ResizeWindow"
}
static void redrawgui()
{
	// TODO Use 'config.style' 

	#define BACKGROUND   RGB(32, 32, 32) // dark mode?
	#define TEXT_COLOR   RGB(235, 235, 235)
	#define HIGHLIGHT    RGB(76, 194, 255) // Sampled from Windows 11 Alt-Tab
	#define HIGHLIGHT_BG RGB(11, 11, 11) // Sampled from Windows 11 Alt-Tab

	#define ICON_WIDTH 64
	#define ICON_PAD    8
	#define HORZ_PAD   24
	#define VERT_PAD   32

	#define SEL_OUTLINE   3
	#define SEL_RADIUS   10
	#define SEL_VERT_OFF 10 // Selection rectangle offset (actual pixel offsets depends on SEL_RADIUS)
	#define SEL_HORZ_OFF  6 // Selection rectangle offset (actual pixel offsets depends on SEL_RADIUS)

	static HBRUSH window_background = {0};
	static HBRUSH selection_background = {0};
	static HPEN selection_outline = {0};

	// Init background brushes (static vars)
	if (window_background == NULL) {
		window_background = CreateSolidBrush(BACKGROUND);
	}
	if (selection_background == NULL) {
		selection_background = CreateSolidBrush(HIGHLIGHT_BG);
	}
	if (selection_outline == NULL) {
		selection_outline = CreatePen(PS_SOLID, SEL_OUTLINE, HIGHLIGHT);
	}

	RECT rect = {0};
	GetClientRect(gui, &rect);

	// Invalidate & draw window background
	RedrawWindow(gui, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
	FillRect(gdi.context, &rect, window_background);

	// Select pen & brush for RoundRect() (used to draw selection rectangle)
	HPEN old_pen = (HPEN)SelectObject(gdi.context, selection_outline);
	HBRUSH old_brush = (HBRUSH)SelectObject(gdi.context, selection_background);

	// Select text font, color & background for DrawTextW() (used to draw app name)
	HFONT old_font = (HFONT)SelectObject(gdi.context, GetStockObject(DEFAULT_GUI_FONT));
	SetTextColor(gdi.context, TEXT_COLOR);
	SetBkMode(gdi.context, TRANSPARENT);

	// This is how GDI handles memory?
	//DeleteObject(old_pen);
	//DeleteObject(old_brush);
	//DeleteObject(old_font);

	for (int i = 0; i < apps_count; i++) {
		app_t *app = &apps[i];

		// Special-case math for index 0
		i32  left0 = i == 0 ? ICON_PAD : 0;
		i32 right0 = i != 0 ? ICON_PAD : 0;

		i32  icons = (ICON_PAD + ICON_WIDTH + ICON_PAD) * i;
		i32   left = HORZ_PAD + left0 + icons + right0;
		i32    top = VERT_PAD;
		i32  width = ICON_WIDTH;
		i32 height = ICON_WIDTH;
		i32  right = left + width;
		i32 bottom = top + height;

		//RECT icon_rect = (RECT){left, top, right, bottom};
		//bool app_is_mouseover = PtInRect(&icon_rect, (POINT){mouse_x, mouse_y});
		//print(L"mouse %s\n", app_is_mouseover ? L"over" : L"not over");

		if (app != selected_app) {
			// Draw only icon, with window background
			DrawIconEx(gdi.context, left, top, app->icon, width, height, 0, window_background, DI_NORMAL);
		} else {
			// Draw selection rectangle and icon, with selection background
			RoundRect(gdi.context, left - SEL_VERT_OFF, top - SEL_HORZ_OFF, right + SEL_VERT_OFF, bottom + SEL_HORZ_OFF, SEL_RADIUS, SEL_RADIUS);
			DrawIconEx(gdi.context, left, top, app->icon, width, height, 0, selection_background, DI_NORMAL);
			
			// Draw app name
			u16 *title = app->appname.text;

			// Measure text width
			RECT test_rect = {0};
			DrawTextW(gdi.context, title, -1, &test_rect, DT_CALCRECT | DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
			i32 text_width = (test_rect.right - test_rect.left) + 2; // Hmm, measured size seems a teensy bit too small, so +2

			// DrawTextW() uses a destination rect for drawing
			RECT text_rect = {0};
			text_rect.left = left;
			text_rect.right = right;
			text_rect.bottom = rect.bottom;

			i32 width_diff = text_width - (text_rect.right - text_rect.left);
			if (width_diff > 0) {
				// text_rect is too small for text, grow the rect
				text_rect.left -= (i32)ceil(width_diff / 2.0);
				text_rect.right += (i32)floor(width_diff / 2.0);
			}

			text_rect.bottom -= ICON_PAD; // *** this lifts the app text up a little ***

			// If the app name, when centered, extends past window bounds, adjust label position to be inside window bounds
			if (text_rect.left < 0) {
				text_rect.right += ICON_PAD + abs(text_rect.left);
				text_rect.left   = ICON_PAD + 0;
			}
			if (text_rect.right > rect.right) {
				text_rect.left -= (text_rect.right - rect.right) + ICON_PAD;
				text_rect.right = rect.right - ICON_PAD;
			}

			///* dbg */ FillRect(gdi.context, &text_rect, CreateSolidBrush(RGB(255, 0, 0))); // Check the size of the text box
			DrawTextW(gdi.context, title, -1, &text_rect, DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
		}
	}
}

static void autorun(bool enabled, string reg_key_name, string launch_args)
{
	// Assemble the target (filepath + args) for the autorun reg key

	string filepath;
	string target;
	
	if (sizeof filepath + sizeof launch_args.length > sizeof target.text) { // TODO eh, do the effing math right!
		print(L"ERROR: Path too long. Can't setup autorun\n");
		return;
	};

	GetModuleFileNameW(NULL, filepath.text, countof(filepath.text)); // Get filepath of current module
	StringCchPrintfW(target.text, countof(target.text)-1, L"\"%s\" %s", filepath, launch_args.text); // "sprintf"

	HKEY reg_key;
	LSTATUS success; // BUG Not checking 'success' below

	if (enabled) {
		success = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_SET_VALUE, NULL, &reg_key, NULL);
		success = RegSetValueExW(reg_key, reg_key_name.text, 0, REG_SZ, target.text, target.length);
		success = RegCloseKey(reg_key);
	} else {
		success = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &reg_key);
		success = RegDeleteValueW(reg_key, reg_key_name.text);
		success = RegCloseKey(reg_key);
	}
}

static i64 KeyboardProc(int code, u64 wparam, i64 lparam)
{
	if (code < 0) { // "If code is less than zero, the hook procedure must pass the message to..." blah blah blah read the docs
		return CallNextHookEx(NULL, code, wparam, lparam);
	}

	// a) There are situations when my kbd hook is not called, for example when elevated processes consume keyboard events.
	// b) I keep internal state for all pressed keys.
	// This leads to stuck keys when the kbd hook is not called when some key is released.
	// (Ctrl-Shift-Esc is an example, and will leave Shift stuck in my internal keyboard state.)
	// There seems to be no elegant solution to this problem except running as admin.
	// Trust me, I did my research. To get you started: https://www.codeproject.com/Articles/716591/Combining-Raw-Input-and-keyboard-Hook-to-selective
	// My solution is to adopt a sync point to recover from bad ("stuck") state.
	if (!selected_window) { // i.e. our switcher is inactive
		synckeys();
	}

	// Info about the keyboard for the low level hook
	KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT *)lparam;
	vkcode key = kbd->vkCode;
	bool key_down = !(kbd->flags & LLKHF_UP);
	bool key_repeat = setkey(key, key_down); // Manually track key repeat using 'keyboard->keys' (see above)

	// Typically:
	//  mod1+key1 = Alt-Tab
	//  mod2+key2 = Alt-Tilde/Backquote (scancode 0x29)
	// TODO Remove mod2 ? Just have one modifier ?
	bool shift_held  = getkey(VK_LSHIFT) || getkey(VK_RSHIFT);
	//print(L"%s", shift_held && !key_repeat ? L"shift\n" : L"");
	bool mod1_held   = getkey(config.mod1);
	bool mod2_held   = getkey(config.mod2);
	bool key1_down   = key == config.key1 && key_down;
	bool key2_down   = key == config.key2 && key_down;
	bool mod1_up     = key == config.mod1 && !key_down;
	bool mod2_up     = key == config.mod2 && !key_down;
	bool esc_down    = key == VK_ESCAPE && key_down;

	// Extended window management functionality
	// TODO Separate up/left & down/right: Use left/right for apps & up/down for windows.
	//      Especially since I think I'm eventually gonna graphically put a vertical
	//      window title list under the horizontal app icon list
	bool enter_down  = key == VK_RETURN && key_down;
	bool delete_down = key == VK_DELETE && key_down;
	bool prev_down   = (key == VK_UP || key == VK_LEFT) && key_down;
	bool next_down   = (key == VK_DOWN || key == VK_RIGHT) && key_down;
	bool ctrl_held   = getkey(VK_LCONTROL) || getkey(VK_RCONTROL); // Only used for disabled on/off toggle hotkey. BUG! What if the users chooses their mod1 or mod2 to be ctrl?
	bool keyQ_down   = key ==  0x51 && key_down;
	bool keyW_down   = key ==  0x57 && key_down;
	bool keyM_down   = key ==  0x4D && key_down;
	bool keyF4_down  = key == VK_F4 && key_down;
	bool keyB_down   = key ==  0x42 && key_down;
	bool keyPrtSc_down = key == VK_SNAPSHOT && key_down;
	bool keyF12_down = key == VK_F12 && key_down;

	// NOTE By returning a non-zero value from the hook procedure, the message
	//      is consumed and does not get passed to the target window
	i64 pass_message = 0;
	i64 consume_message = 1;

	// TODO Support:
	// [x] Alt-Tab to cycle apps (configurable hotkey)
	// [x] Alt-Tilde/Backquote to cycle windows (configurable hotkey)
	// [x] Shift to reverse direction
	// [x] Esc to cancel
	// [x] Restore window on cancel
	// [x] Arrow keys to cycle apps
	// [x] Enter to switch app
	// [x] Wrap bump on key repeat
	// [x] Input sink so keys don't bleed to underlying apps
	// [x] Flicker-free fast switch
	// [ ] Q to quit application (i.e. close all windows, like macOS)
	// [x] W to close window (like Windows and macOS)
	// [x] M to minimize/restore window (what about (H)iding?)
	// [x] Delete to close window (like Windows 10 and up)
	// [x] Alt-F4 (atm closes cmdtab.exe)
	// [ ] Mouse click app icon to switch
	// [x] Cancel switcher on mouse click outside
	// [ ] Close app with mouse click on a red "X" on icon
	// [ ] Show number of subwindows for each app
	// [ ] Show numbers on each app and allow quick jumping by pressing that number
	// [-] BUGGY! A hotkey to disable/enable
	// [ ] Support dark/light mode
	// [ ] Ctrl+Alt-Tab for "persistent" GUI

	// BUG! Can't use Ctrl+Alt-Tab, because this is the "persistent alt tab" hotkey
	// Ctrl+Alt-Tab (has to be high up since the condition checking conflicts with Alt-Tab (cuz Alt-Tab doesn't check for !ctrl_held)
	// BUG! This is really buggy when passing between Windows' Alt-Tab and ours
	//      Hanging modifiers, missing modifiers...
	//      Passing from Windows' Alt-Tab to ours doesn't even work (oh wait, is that because Ctrl+Alt-Tab is the persistent alt tab hotkey?)
	if (key1_down && mod1_held && ctrl_held) {
		#if 0
		bool enabled = config.switch_apps = !config.switch_apps; // NOTE Atm, I only support a hotkey for toggling app switching, not toggling window switching (cuz why would we?)
		if (!enabled) {
			cancel(, false);
			return pass_message;
		} else {
			// pass-through to Alt-Tab below
		}
		#endif
	}

	if (selected_window && keyF12_down) {
		debug();
	}

	// ========================================
	// Activation
	// ========================================
	
	// First Alt-Tab
	if (!selected_window && key1_down && mod1_held && config.switch_apps) {
		activate(); // Sets 'selected_window' and 'selected_app
		// NOTE No return or goto, as these two activations fallthrough to navigation below
	}
	// First Alt-Tilde/Backquote
	if (!selected_window && key2_down && mod2_held && config.switch_windows) {
		activate(); // Sets 'selected_window' and 'selected_app'
		selectnext(false, shift_held, !config.wrap_bump || !key_repeat);
		// NOTE No return or goto, as these two activations fallthrough to navigation below
	}

	// ========================================
	// Navigation
	// ========================================

	// Alt-Tab - next app (hold Shift for previous)
	if (selected_window && key1_down && mod1_held) {
		selectnext(true, shift_held, !config.wrap_bump || !key_repeat);
		if (config.show_gui_for_apps) {
			resizegui(); // NOTE Must call resizegui after activate, before redrawgui & before showgui
			redrawgui();
			showgui(config.show_gui_delay);
		}
		if (config.fast_switching_apps) {
			showselected();
			//preview(*selected_window); // Not implemented yet
		}

		goto consume_message; // Since I "own" this hotkey system-wide, I consume this message unconditionally
	}

	// Alt-Tilde/Backquote - next window (hold Shift for previous)
	if (selected_window && key2_down && mod2_held) {
		if (foregrounded_window) {
			selectnext(false, shift_held, !config.wrap_bump || !key_repeat);
			//print(L"Alt-Tilde/Backquoteing after having Alt-Tilde/Backquoteed\n");
		} else {
			//print(L"Alt-Tilde/Backquoteing after having Alt-Tabbed\n");
		}

		if (config.show_gui_for_windows) {
			resizegui(); // NOTE Must call resizegui after activate, before redrawgui & before showgui
			redrawgui();
			showgui(config.show_gui_delay);
		}
		if (config.fast_switching_windows) {
			showselected();
			//preview(*selected_window); // Not implemented yet
		}

		goto consume_message; // Since I "own" this hotkey system-wide, I consume this message unconditionally
	}

	// Alt-Arrows. People ask for this, so...
	// Copy/paste Alt-Tab above (minus prev_down)
	if (selected_window && (next_down || prev_down) && mod1_held) { // selected_window: You can't *start* window switching with Alt-Arrows, but you can navigate the switcher with arrows when switching is active
		selectnext(true, prev_down, !config.wrap_bump || !key_repeat);
		if (config.show_gui_for_apps) {
			resizegui(); // NOTE Must call resizegui after activate, before redrawgui & before showgui
			redrawgui();
			showgui(config.show_gui_delay);
		}
		if (config.fast_switching_apps) {
			showselected();
			//preview(*selected_window); // Not implemented yet
		}

		goto consume_message; // Since our swicher is active, I "own" this hotkey, so I consume this message
	}

	// ========================================
	// Deactivation
	// ========================================

	// Alt keyup - switch to selected window
	// BUG! Aha! Lemme tell yalittle secret: Our low level hook doesn't observe "Alt keydown" because Alt can be pressed down for
	//      so many purposes on system level. I'm only interested in Alt-Tab, which means that I only really check for "Tab keydown",
	//      and then transparently check if Alt was already down when Tab was pressed.
	//      This means that I let all "Alt keydown"s pass through our hook, even the "Alt keydown" that the user pressed right before
	//      pressing Tab, which is sort of "ours", but I passed it through anyways (can't consume it after-the-fact).
	//      The result is that there is a hanging "Alt down" that got passed to the foreground app which needs an "Alt keyup", or
	//      the Alt key gets stuck. Thus, I always pass through "Alt keyup"s, even if I used it for some functionality.
	//      In short:
	//      1) Our app never hooks Alt keydowns
	//      2) Our app hooks Alt keyups, but always passes it through on pain of getting a stuck Alt key because of point 1
	if (selected_window && (mod1_up || mod2_up)) {
		showselected();
		cancel(false);
		goto pass_message; // See note above on "Alt keyup" on why I don't consume this message
	}
	// Alt-Enter - switch to selected window
	if (selected_window && (mod1_held || mod2_held) && enter_down) {
		showselected();
		cancel(false);
		goto consume_message; // Since our swicher is active, I "own" this hotkey, so I consume this message
	}
	// Alt-Esc - cancel switching and restore initial window
	if (selected_window && (mod1_held || mod2_held) && esc_down) { // BUG There's a bug here if mod1 and mod2 aren't both the same key. In that case I'll still pass through the key event, even though I should own it since it's not Alt-Esc
		cancel(config.restore_on_cancel);
		goto consume_message; // Since our swicher is active, I "own" this hotkey, so I consume this message
	}

	// ========================================
	// Extended window management
	// ========================================
	
	// Alt-F4 - quit cmdtab
	if (selected_window && ((mod1_held || mod2_held) && keyF4_down)) {
		close(gui);
		goto consume_message;
	}
	// Alt-Q - close all windows of selected application
	if (selected_window && ((mod1_held || mod2_held) && keyQ_down)) {
		print(L"Q\n");
		goto consume_message;
	}
	// Alt-W or Alt-Delete - close selected window
	if (selected_window && ((mod1_held || mod2_held) && (keyW_down || delete_down))) {
		close(*selected_window);
		activate();
		resizegui(); // NOTE Must call resizegui after activate, before redrawgui & before showgui
		redrawgui();
		goto consume_message;
	}
	// Alt-M - minimize selected window
	if (selected_window && ((mod1_held || mod2_held) && keyM_down)) {
		minimize(*selected_window);
		goto consume_message;
	}
	// Alt-B - display app name and window class to user, to add in blacklist
	if (selected_window && ((mod1_held || mod2_held) && keyB_down)) {
		snitch(*selected_window);
		cancel(false);
		goto consume_message;
	}
	// Input sink - consume keystrokes when activated
	if (selected_window && (mod1_held || mod2_held) && !keyPrtSc_down) {
		goto consume_message;
	}

	pass_message:
		return CallNextHookEx(NULL, code, wparam, lparam);
	consume_message:
		return consume_message;
}
static i64 WndProc(handle hwnd, u32 message, u64 wparam, i64 lparam)
{
	switch (message) {
		case WM_ERASEBKGND: {
			return 1;
		}
		case WM_ACTIVATE: { // WM_ACTIVATEAPP 
			// Cancel on mouse clicking outside window
			if (wparam == WA_INACTIVE) {
				print(L"gui lost focus\n");
				//return 1; // Nu-uh!
				cancel(false); // NOTE See comments in cancel
			} else {
				print(L"gui got focus\n");
				//return 0;
				//synckeys();
			}
			break;
		}
		case WM_PAINT: {
			// Double buffered drawing:
			// I draw off-screen in-memory to 'gdi.context' whenever changes occur,
			// and whenever I get the WM_PAINT message I just blit the
			// off-screen buffer to screen ("screen" means the 'hdc' returned by
			// BeginPaint() passed our 'hwnd')
			PAINTSTRUCT ps = {0};
			handle context = BeginPaint(hwnd, &ps);
			BitBlt(context, 0, 0, gdi.rect.right - gdi.rect.left, gdi.rect.bottom - gdi.rect.top, gdi.context, 0, 0, SRCCOPY);
			//int scale = 1.0;
			//SetStretchBltMode(gdi.context, HALFTONE);
			//StretchBlt(hdc, 0, 0, gdi.width, gdi.height, gdi.context, 0, 0, gdi.width * scale, gdi.height * scale, SRCCOPY);
			EndPaint(hwnd, &ps);
			ReleaseDC(hwnd, context);
			return 0;
		}
		case WM_MOUSEMOVE: {
			mouse_x = LOWORD(lparam);
			mouse_y = HIWORD(lparam);
			print(L"x%i y%i\n", mouse_x, mouse_y);
			TrackMouseEvent(&(TRACKMOUSEEVENT){.cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = gui});
			break;
		}
		case WM_MOUSELEAVE: {
			mouse_x = 0;
			mouse_y = 0;
			print(L"x%i y%i\n", mouse_x, mouse_y);
			break;
		}
		case WM_CLOSE: {
			cancel(false);
			if (!ask(L"Quit cmdtab?")) {
				return 0;
			}
			break;
		}
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;
		}
	}
	return DefWindowProcW(hwnd, message, wparam, lparam);
}
static i64 ShellProc(int code, u64 wparam, i64 lparam)
{
	if (code < 0) { // "If code is less than zero, the hook procedure must pass the message to..." blah blah blah read the docs
		return CallNextHookEx(NULL, code, wparam, lparam);
	}
	switch (code) {
	case HSHELL_ACTIVATESHELLWINDOW:
	case HSHELL_WINDOWCREATED:
	case HSHELL_WINDOWDESTROYED: {
		HWND hwnd = (HWND)wparam;

		print(L"yo!");
	}
	}
	return 0;
}
static void WinEventProc(handle hook, u32 event, handle hwnd, i32 objectid, i32 childid, u32 threadid, u32 timestamp) 
{
	if (event == EVENT_SYSTEM_FOREGROUND) {
		//print(L"FOREGROUND %i, %i: ", objectid, childid);
		//printhwnd(hwnd);
		//PlaySoundW(L"C:\\Windows\\Media\\Speech Misrecognition.wav", NULL, SND_FILENAME | SND_ASYNC);
	}
	//if (event == EVENT_OBJECT_DESTROY && hwnd && (hwnd = getuwp(hwnd)) == GetAncestor(hwnd, GA_ROOT) && !ignored(hwnd)) {
	if (event == EVENT_OBJECT_DESTROY && (hwnd = getuwp(hwnd)) && !ignored(hwnd)) {
		//print(L"DESTROY %i, %i: ", objectid, childid);
		//printhwnd(hwnd);
		//PlaySoundW(L"C:\\Windows\\Media\\Windows Information Bar.wav", NULL, SND_FILENAME | SND_ASYNC);
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {

//#ifdef _DEBUG
//	FILE *conout = NULL;
//	AllocConsole();
//	freopen_s(&conout, "CONOUT$", "w", stdout);
//#endif

	// Prompt about autorun (unless I got autorun arg)
	string autorunarg = string(L"--autorun");
	if (wcscmp(pCmdLine, autorunarg.text)) { // lstrcmpW, CompareStringW
		string regkeyname = string(L"cmdtab");
		bool shouldautorun = ask(L"Start cmdtab automatically?\nRelaunch cmdtab.exe to change your mind.");
		autorun(shouldautorun, regkeyname, autorunarg);
	}

	// Quit if another instance of cmdtab is already running
	handle hMutex = CreateMutexW(NULL, true, L"cmdtab_mutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		MessageBoxW(NULL, L"cmdtab is already running.", L"cmdtab", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
		return 0; // 0
	}

	// Initialize runtime-dependent settings. This one is dependent on user's current kbd layout
	config.key2 = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX);

	// Install (low level) keyboard hook
	handle hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, (HOOKPROC)KeyboardProc, NULL, 0);
	// Install window event hook for one event: foreground window changes. Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20130930-00/?p=3083
	//handle hShellHook = SetWindowsHookExW(WH_SHELL, (HOOKPROC)ShellProc, NULL, 0);
	handle hWinEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
	//handle hWinEventHook2 = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
	
	// Create window
	WNDCLASSEXW wcex = {0};
	wcex.cbSize = sizeof wcex;
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)WndProc;
	wcex.hInstance = hInstance;
	wcex.lpszClassName = L"cmdtab";

	gui = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, RegisterClassExW(&wcex), NULL, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

	// Clear all window styles for very plain window
	SetWindowLongW(gui, GWL_STYLE, 0);
	
	// SetThemeAppProperties(NULL); // lol x)
	
	// Use dark mode to make the title bar dark
	// The alternative is to disable the title bar but this removes nice rounded
	// window shaping.
	// For dark mode details, see: https://stackoverflow.com/a/70693198/659310
	//BOOL USE_DARK_MODE = TRUE;
	//DwmSetWindowAttribute(gui, DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE, sizeof USE_DARK_MODE);

	// Rounded window corners
	DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
	DwmSetWindowAttribute(gui, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof preference);

	// Start msg loop for thread
	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0) > 0) {
		DispatchMessageW(&msg);
	}

	// Done
	UnhookWindowsHookEx(hKeyboardHook);
	//UnhookWinEvent(hWinEventHook);
	//UnhookWinEvent(hWinEventHook2);
	//CloseHandle(hKeyboardHook);
	if (hMutex) {
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
	}
	//DestroyWindow(gui);

	print(L"done\n");
	_CrtDumpMemoryLeaks();
	return (int)msg.wParam;
}
