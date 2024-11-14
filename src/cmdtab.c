#define WINDOWS_LEAN_AND_MEAN
#define COBJMACROS
#define _CRTDBG_MAP_ALLOC

//#include <stdlib.h>
//#include <stdbool.h>
#include <string.h>
#include <stdarg.h>    // Used by print
#include <limits.h>	   /* for CHAR_BIT */
#include <stddef.h>
#include <math.h>

#include <windows.h>


#include <crtdbg.h>

//#include <malloc.h>
//#include <memory.h>

//#include <wchar.h>     // We're wiiiide, baby!
#include "WinUser.h"   // Used by gettitle, getpath, filter, link, next
#include "processthreadsapi.h" // Used by getpath
#include "WinBase.h"   // Used by getpath
#include "handleapi.h" // Used by getpath
#include "Shlwapi.h"   // Used by getname
#include "dwmapi.h"    // Used by filter
#include "strsafe.h"   // Used by getappname
#include "pathcch.h"   // Used by getfilename
#include "commoncontrols.h" // Used by geticon, needs COBJMACROS

//==============================================================================
// Linker directives
//==============================================================================

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib") 
#pragma comment(lib, "dwmapi.lib") 
#pragma comment(lib, "version.lib") // Used by appname
#pragma comment(lib, "pathcch.lib")
#pragma comment(lib, "winmm.lib") // For PlaySound, TODO Remove

//==============================================================================
// Utility macros
//==============================================================================

#define debug()    __debugbreak() // Or DebugBreak in debugapi.h
#define countof(a) (ptrdiff_t)(sizeof(a) / sizeof(*a))

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
//typedef struct {int _;}   *handle;
typedef void *             handle;
typedef struct tagRECT     rect;
typedef unsigned int       vkcode;

//#define MAX_PATH 260

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
typedef struct linked_window linked_window;
typedef struct gdi gdi_t;
typedef struct identifier identifier;
typedef struct config config_t;

#define string(str) (string){ str, countof(str)-1, true } // TODO: -1 to remove NUL-terminator?

struct string {
	u16 text[(MAX_PATH*2)+1];
	u32 length;
	bool ok;
};

// struct txt {
//	 u16 len;
//	 u16 *txt;
// };
// txtcmp
// txteq
// txtcpy
// txtcat
// txtchr, txtrchr
// txtstr, txtrstr


struct linked_window {
	handle hwnd;
	handle icon;
	string filepath;
	string filename;
	string appname;
	linked_window *next_window;
	linked_window *previous_window;
	linked_window *next_app;
	linked_window *previous_app;
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
	bool ignore_minimized; // TODO? Atm, 'ignore_minimized' affects both Alt+` and Alt+Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.) TODO Should this be called 'restore_minimized'?
	// Appearance
	u32 gui_height;
	u32 gui_horz_margin;
	u32 gui_vert_margin;
	u32 icon_width;
	u32 icon_horz_padding;
	// Blacklist
	struct identifier {
		u16 *filename; // File name without extension, ex. "explorer"
		u16 *class;    // Window class
	} blacklist[32];
};

//==============================================================================
// App state
//==============================================================================

static handle gui;                          // Handle for main window (switcher)
static linked_window windows[256];          // Filtered windows that can be switched to, and, if a top-level window, displayed in switcher
static size windows_count;                  // Number of elements in 'windows' array
static size apps_count;                     // Number of top-level windows (i.e. applications) in 'windows' array
static linked_window *window_highlighted;   // Currently selected window in switcher
static linked_window *window_foregrounded;  // Last switched-to window
static handle window_restore;               // Whatever window was switched away from. Does not need to be in filtered 'windows'
static u8 keyboard[32];                     // 256 bits for tracking key repeat when using low-level keyboard hook
static gdi_t gdi;                           // Off-screen bitmap used for double-buffering
static i32 mouse_x;
static i32 mouse_y;

static bool EnumWindowsProc(handle, i64);     // Used as callback for EnumWindows
static bool EnumChildProc(handle, i64);       // Used as callback for EnumChildWindows
static void TimerProc(handle, u32, u64, u32); // Forward declare to resolve circular dependency between showafter and TimerProc

static string UWPHostExecutable = { L"ApplicationFrameHost.exe", countof(L"ApplicationFrameHost.exe")-1, 1 };
static string UWPHostClass = { L"ApplicationFrameWindow", countof(L"ApplicationFrameWindow")-1, 1 };
static string UWPHostedClass = { L"Windows.UI.Core.CoreWindow", countof(L"Windows.UI.Core.CoreWindow")-1, 1 };

static config_t config = {
	// The default key2 is scan code 0x29 (hex) / 41 (decimal)
	// This is the key that is physically below Esc, left of 1 and above Tab
	// WARNING!
	// VK_OEM_3 IS NOT SCAN CODE 0x29 FOR ALL KEYBOARD LAYOUTS 
	// .key2 = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX) MUST BE CALLED DURING RUNTIME INITIALIZATION
	
	// Hotkeys
	.mod1 = VK_LMENU,
	.key1 = VK_TAB,
	.mod2 = VK_LMENU,
	.key2 = VK_OEM_3,
	// Behavior
	.switch_apps            = true,
	.switch_windows         = true,
	.fast_switching_apps    = true, // default: false
	.fast_switching_windows = true,
	.show_gui_for_apps      = true,
	.show_gui_for_windows   = true,
	.show_gui_delay         = 0,
	.wrap_bump              = true,
	.ignore_minimized       = false,
	// Appearance
	.gui_height        = 128,
	.icon_width        =  64,
	.icon_horz_padding =   8,
	.gui_horz_margin   =  24,
	.gui_vert_margin   =  32,
	// Blacklist
	.blacklist = {
		//{ NULL,                  L"Windows.UI.Core.CoreWindow" },
		//{ NULL,                  L"Xaml_WindowedPopupClass" },
		//{ L"explorer",             L"ApplicationFrameWindow" },
		{ NULL,             L"ApplicationFrameWindow" },
		//{ L"explorer",           L"ApplicationManager_DesktopShellWindow" },
		//{ L"ApplicationFrameHost", L"ApplicationFrameWindow" },
		//{ L"TextInputHost",        NULL },
		//{ NULL ,                 L"ApplicationFrameHost" },
	},
};

//==============================================================================
// Here we go!
//==============================================================================

static void print(u16 *fmt, ...)
{
	// TODO #ifndef _DEBUG this away for release builds?
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	OutputDebugStringW(buffer);
}
static void copy(u16 *src, string *dst)
{
	
}
static bool equals(string *s1, string *s2)
{
	// Compare strings for equality.
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
static bool endswith(string *str, string *fix) 
{
	size str_len = str->length-1;
	size fix_len = fix->length-1;
	return str_len >= fix_len && !memcmp(str->text+str_len-fix_len, fix->text, fix_len);
}

static string getclass(handle hwnd)
{
	string class = {0};
	class.length = GetClassNameW(hwnd, class.text, countof(class.text));
	class.ok = (class.length > 0);
	return class;
}
static string gettitle(handle hwnd)
{
	string title = {0};
	title.length = GetWindowTextW(hwnd, title.text, countof(title.text));
	title.ok = (title.length > 0);
	return title;
}
static handle getuwp(handle hwnd) {
	string class = getclass(hwnd);
	if (!equals(&class, &UWPHostClass)) {
		return hwnd;
	}
	u32 pid;
	GetWindowThreadProcessId(hwnd, &pid);
	print(L"pid1 %u %s %s\n", pid, gettitle(hwnd).text, getclass(hwnd).text);
	handle hwnd2 = hwnd;
	EnumChildWindows(hwnd, EnumChildProc, &hwnd2);
	string class2 = getclass(hwnd2);

	//if (hwnd2 != hwnd) {
		return hwnd2;
	//} else {
	//	return hwnd;
		//return NULL;
	//}
}
static u32 getpid(handle hwnd)
{
	u32 pid1;
	GetWindowThreadProcessId(hwnd, &pid1);
	string class = getclass(hwnd);
	if (!equals(&class, &UWPHostClass)) {
		return pid1;
	}
	u32 pid2 = pid1;
	print(L"pid1 %u %s\n", pid2, class.text);
	EnumChildWindows(hwnd, EnumChildProc, &pid2);
	if (pid2 != pid1) {
		return pid2;
	} else {
		return 0;
	}
}
static string getfilepath(handle hwnd)
{
	// How to get the filepath of the executable for the process that spawned the window:
	// 1. GetWindowThreadProcessId() to get process id for window
	// 2. OpenProcess() for access to remote process memory
	// 3. QueryFullProcessImageNameW() [Windows Vista] to get full "executable image" name (i.e. exe path) for process
	
	string path = {0};

	u32 pid;// = getpid(hwnd); // will be 0 for UWP hosts
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

	//// https://github.com/microsoft/PowerToys/blob/00ee6c15100b3b413eee9c2d59a3c3c00e4f3bf1/src/modules/launcher/Plugins/Microsoft.Plugin.WindowWalker/Components/WindowProcess.cs#L141

	if (0 && endswith(&path, &UWPHostExecutable)) {
		handle hwnd2 = hwnd;
		print(L"pid1 %u %s %s\n", pid, gettitle(hwnd).text, getclass(hwnd).text);
		EnumChildWindows(hwnd, EnumChildProc, &hwnd2);
		if (hwnd2 != hwnd) {
			return getfilepath(hwnd2);
		}
		/*
		u32 pid2 = pid;
		EnumChildWindows(hwnd, EnumChildProc, &pid2);
		if (pid2 != pid) {
			return getfilepath(pid2);
		} else {
			return 0;
		}
		*/
	}
	return path;
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

	name.length = length+1; // ? Fixes #2 ¯\_(ツ)_/¯
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
static string getfilename(u16 *filepath, bool extension)
{
	// Just yer normal filepath string manipulation
	string name = {0};
	name.ok = SUCCEEDED(StringCchCopyNW(name.text, countof(name.text), PathFindFileNameW(filepath), countof(name.text)));
	if (!extension) {
		name.ok = name.ok && SUCCEEDED(PathCchRemoveExtension(name.text, countof(name.text)));
	}
	name.ok = name.ok && SUCCEEDED(StringCchLengthW(name.text, countof(name.text), &name.length));
	return name;
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
	// Adding these for good measure. They seem to solve stuck keys at least some of the time
	//keybd_event(VK_MENU,    0, KEYEVENTF_KEYUP, 0);
	//keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
	//keybd_event(VK_SHIFT,   0, KEYEVENTF_KEYUP, 0);
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
static void show(handle hwnd)
{
	if (IsIconic(hwnd)) {
		ShowWindow(hwnd, SW_RESTORE);
	} else {
		ShowWindow(hwnd, SW_SHOWNA);
	}

	// https://github.com/microsoft/PowerToys/blob/7d0304fd06939d9f552e75be9c830db22f8ff9e2/src/modules/fancyzones/FancyZonesLib/util.cpp#L376
	// This is a hack to bypass the restriction on setting the foreground window
	// gg windows ¯\_(ツ)_/¯
	SendInput(1, &(INPUT){.type = INPUT_MOUSE}, sizeof(INPUT));

	//keybd_event(VK_MENU, 0, 0, 0);
	SetForegroundWindow(hwnd);
	//keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
}
static void showafter(handle hwnd, u32 delay)
{
	KillTimer(hwnd, /*timerid*/ 1);
	if (delay > 0) {
		SetTimer(hwnd, /*timerid*/ 1, delay, TimerProc);
		return;
	}
	show(hwnd);
}
static void TimerProc(handle hwnd, u32 msg, u64 eventid, u32 time)
{
	showafter(hwnd, 0);
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
static void snitch()
{
	// Tell user the executable name and class so they can add it to the blacklist
	string path = getfilepath(window_highlighted->hwnd);
	string name = getfilename(path.text, false);
	string class = getclass(window_highlighted->hwnd);

	u16 buffer[2048];
	StringCchPrintfW(buffer, countof(buffer)-1, L"{%s, %s}\n\nAdd to blacklist (TODO!)?", name.text, class.text);

	int result = MessageBoxW(gui, buffer, L"cmdtab", MB_YESNO | MB_ICONASTERISK | MB_TASKMODAL);
	return result == IDOK || result == IDYES;
}

static bool filtered(handle hwnd)
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
	// in the dialog that appears when the user presses ALT+TAB." (docs)
	if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
		return true;
	}
	if (config.ignore_minimized && IsIconic(hwnd)) {
		return true;
	}
	return false;
}
static bool blacklisted(string *filename, string *class)
{
	#define wcseq(str1, str2) (wcscmp(str1, str2) == 0)
	// Iterate blacklist. I allow three kinds of entries in the black list:
	// 1) {name,class} both name and class are specified and must match
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
#define wcseq(str1, str2) (wcscmp(str1, str2) == 0)

	if (wcseq(gettitle(hwnd).text, L"Settings")) {
		int i = 0;
	}
	if (!(hwnd = getuwp(hwnd))) {
		return;
	}

	if (filtered(hwnd)) {
		return;
	}

	string filepath = getfilepath(hwnd);
	string filename = getfilename(filepath.text, false);

	if (!filepath.ok || !filename.ok) {
		print(L"whoops, bad exe path?! %p\n", hwnd);
		return;
	}

	//// https://github.com/microsoft/PowerToys/blob/00ee6c15100b3b413eee9c2d59a3c3c00e4f3bf1/src/modules/launcher/Plugins/Microsoft.Plugin.WindowWalker/Components/WindowProcess.cs#L141
	//if (endswith(&filename, &string(L"ApplicationFrameHost"))) {
	//	u32 pid;
	//	GetWindowThreadProcessId(hwnd, &pid);
	//	print(L"%u %s\n", pid, getclass(hwnd).text);
	//	EnumChildWindows(hwnd, EnumChildProc, (i64)pid);
	//	return;
	//}

	string class = getclass(hwnd);

	if (blacklisted(&filename, &class)) {
		return;
	}

	linked_window *window = &windows[windows_count++];
	window->hwnd = hwnd;
	window->icon = geticon(&filepath.text); 
	window->filepath = filepath;
	window->filename = filename;
	window->appname = getappname(&filepath.text);
	window->next_window = NULL;
	window->previous_window = NULL;
	window->next_app = NULL;
	window->previous_app = NULL;

	if (windows_count == 1) {
		apps_count = 1; // apps_count is incremented below but needs to be init'd here
	}
	if (windows_count < 2) {
		return; // Skip linking when there is only 1 or 0 windows
	}

	// Check if new window2 is same app as a previously added window1 and link them if so
	linked_window *window1 = &windows[0];
	linked_window *window2 = window;
	while (window1) {
		if (equals(&window1->filepath, &window2->filepath)) {
			while (window1->next_window) { window1 = window1->next_window; } // Get last subwindow of window1
			window1->next_window = window2;
			window2->previous_window = window1;
			break;
		}
		if (window1->next_app) {
			window1 = window1->next_app; // Check next top window
		} else {
			break;
		}
	}
	// window2 was not assigned a previous window of the same app, thus window2 is a separate app
	if (window2->previous_window == NULL) {
		window2->previous_app = window1;
		window1->next_app = window2;
		apps_count++;
	}
}
static void reinit()
{
	for (int i = 0; i < windows_count; i++) {
		if (windows[i].icon) {
			DestroyIcon(windows[i].icon);
		}
	}
	windows_count = 0;
	apps_count = 0;
	
	//synckeys();

	// Rebuild our whole window list
	EnumWindows(EnumWindowsProc, 0); // EnumWindowsProc calls filter and add
}
static bool EnumWindowsProc(handle hwnd, i64 lparam)
{
	addwindow(hwnd);
	return true; // Return true to continue EnumWindows
}


static bool EnumChildProc(handle hwnd, i64 lparam)
{
	u32 pid;
	GetWindowThreadProcessId(hwnd, &pid);
	print(L"pid2  %u %s %s\n", pid, gettitle(hwnd).text, getclass(hwnd).text);
	string class = getclass(hwnd);
	if (equals(&class, &UWPHostedClass)) {
		*(handle *)lparam = hwnd;
		return false;
	} else {
		return true; // Return true to continue EnumChildWindows
	}

	/*
	u32 pid;
	GetWindowThreadProcessId(hwnd, &pid);
	print(L"pid2  %u %s\n", pid, getclass(hwnd).text);
	if (*(u32 *)lparam != pid) {
		*(u32 *)lparam = pid;
		return false;
		//addwindow(hwnd);
	}
	return true; // Return true to continue EnumChildWindows
	*/
}

static void print_hwnd(handle hwnd, bool details) {
	if (hwnd) {
		string exe_path = getfilepath(hwnd);
		if (exe_path.ok) return;
		string filename = getfilename(exe_path.text, false);
		string window_title = gettitle(hwnd);
		u16 *ws_iconic = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_ICONIC) ? L" (minimized)" : L"";
		print(L"%s%s \"%s\"\n", filename.text, ws_iconic,  window_title.text, exe_path.text);
		if (details) {
			u16 *ws_visible             = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_VISIBLE) ? L"visible" : L"";
			u16 *ws_child               = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) ? L", child" : L"";
			u16 *ws_disabled            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DISABLED) ? L", disabled" : L"";
			u16 *ws_popup               = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUP) ? L", popup" : L"";
			u16 *ws_popupwindow         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUPWINDOW) ? L", popupwindow" : L"";
			u16 *ws_tiled               = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_TILED) ? L", tiled" : L"";
			u16 *ws_tiledwindow         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_TILEDWINDOW) ? L", tiledwindow" : L"";
			u16 *ws_overlapped          = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPED) ? L", overlapped" : L"";
			u16 *ws_overlappedwindow    = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW) ? L", overlappedwindow" : L"";
			u16 *ws_dlgframe            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DLGFRAME) ? L", dlgframe" : L"";
			u16 *ex_toolwindow       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ? L", ex_toolwindow" : L"";
			u16 *ex_appwindow        = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) ? L", ex_appwindow" : L"";
			u16 *ex_dlgmodalframe    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_DLGMODALFRAME) ? L", ex_dlgmodalframe" : L"";
			u16 *ex_layered          = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) ? L", ex_layered" : L"";
			u16 *ex_noactivate       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) ? L", ex_noactivate" : L"";
			u16 *ex_palettewindow    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_PALETTEWINDOW) ? L", ex_palettewindow" : L"";
			u16 *ex_overlappedwindow = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_OVERLAPPEDWINDOW) ? L", ex_overlappedwindow" : L"";

			u16 class_name[MAX_PATH] = {0};
			GetClassNameW(hwnd, class_name, countof(class_name)-1);

			print(L"\t");
			print(L"%s (%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s) \n",
				class_name,
				ws_visible, ws_child, ws_disabled, ws_popup, ws_popupwindow, ws_tiled, ws_tiledwindow, ws_overlapped, ws_overlappedwindow, ws_dlgframe,
				ex_toolwindow, ex_appwindow, ex_dlgmodalframe, ex_layered, ex_noactivate, ex_palettewindow, ex_overlappedwindow);
		}		
	} else {
		print(L"NULL hwnd\n");
	}
}
static void print_window(linked_window *window) {
	if (window && window->hwnd) {
		u16 *ws_iconic = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_ICONIC) ? L" (minimized)" : L"";
		string class = getclass(window->hwnd);
		string title = gettitle(window->hwnd);
		print(L"%s%s \"%s\" - %s\n", window->filename.text, ws_iconic,  title.text, class.text);
	} else {
		print(L"NULL window\n");
	}
}
static void print_all(bool fancy) {
	double mem = (double)(
		sizeof windows +
		sizeof windows_count +
		sizeof apps_count +
		sizeof window_highlighted +
		sizeof window_foregrounded +
		sizeof window_restore +
		sizeof keyboard +
		//sizeof style + // TDODO Depends on whether style is in config_t or not
		sizeof gdi +
		sizeof config) / 1024; // TODO Referencing global vars
	print(L"%i Alt-Tab windows (%.0fkb mem):\n", windows_count, mem);

	if (!fancy) {
		// Just dump the array
		for (int i = 0; i < windows_count; i++) {
			linked_window *window = &windows[i];
			print(L"%i ", i);
			print_window(window);
		}
	} else {
		// Dump the linked list
		linked_window *top_window = &windows[0]; // windows[0] is always a top window because it is necessarily the first window of its app in the list
		linked_window *sub_window = top_window->next_window;
		while (top_window) {
			print(L"+ ");
			print_window(top_window);
			while (sub_window) {
				print(L"\t- ");
				print_window(sub_window);
				sub_window = sub_window->next_window;
			}
			if (top_window->next_app) {
				top_window = top_window->next_app;
				sub_window = top_window->next_window;
			} else {
				break;
			}
		}
	}
	print(L"\n");
	print(L"________________________________\n");
}

static linked_window *find_first(linked_window *window, bool top_level) {
	if (top_level) {
		while (window->previous_window) { window = window->previous_window; } // 1. Go to top window
		while (window->previous_app) { window = window->previous_app; } // 2. Go to first top widndow
	} else {
		while (window->previous_window) { window = window->previous_window; } // 1. Go to top window (which is also technically "first sub window")
	}
	return window;
}
static linked_window *find_last(linked_window *window, bool top_level) {
	if (top_level) {
		while (window->previous_window) { window = window->previous_window; } // 1. Go to top window
		while (window->next_app) { window = window->next_app; } // 2. Go to last top window
	} else {
		while (window->next_window) { window = window->next_window; } // 1. Go to last sub window
	}
	return window;
}
static linked_window *find_next(linked_window *window, bool top_level, bool reverse, bool wrap) {
	if (window == NULL) {
		print(L"window is NULL");
		return NULL;
	}

	if (top_level) {
		// Ensure that 'window' is its app's top window
		window = find_first(window, false);
	}

	linked_window *next_window = NULL;

	if (top_level) {
		next_window = reverse ? window->previous_app : window->next_app;
	} else {
		next_window = reverse ? window->previous_window : window->next_window;
	}

	if (next_window) {
		return next_window;
	}

	// No next window. Should we wrap around?

	if (!wrap) { 
		return window;
	}

	bool alone;

	if (top_level) {
		// Are we the only top level window? All alone on the top :(
		alone = !window->next_app && !window->previous_app; 
	} else {
		// Are we the only window of this app? Top looking for sub ;)
		alone = !window->next_window && !window->previous_window; 
	}

	if (!alone) { // '!next_window && wrap' implied
		return reverse ? find_last(window, top_level) : find_first(window, top_level);
	}

	return window;
}

static void select_foreground() {
	window_highlighted = windows_count > 0 ? &windows[0] : NULL;
	window_restore = GetForegroundWindow();

	if (window_highlighted == NULL) {
		// The foreground window did not pass filter
	}
	
	if (window_highlighted && window_highlighted->hwnd != window_restore)  {
		// The restore window did not pass filter
	}
	
	///*dbg*/ print(L"start: "); print_window(window_highlighted);
	///*dbg*/ print(L"start: "); print_hwnd(window_restore, true);
}
static void select_next(bool top_level, bool reverse, bool wrap) {
	if (!window_highlighted) {
		debug();
	}
	window_highlighted = find_next(window_highlighted, top_level, reverse, wrap);
}
static void showhighlighted() {
	///*dbg*/ print(L"end: "); print_window(window_highlighted);
	///*dbg*/ print(L"end: "); print_hwnd(window_highlighted->hwnd, true);
	window_foregrounded = window_highlighted;
	show(window_foregrounded->hwnd);
}
static void cancel(bool restore)  {
	// NOTE I strive to keep selection state and gui state independent, but in the
	//      case of cancellation there is a dependence between the two, encapsulated
	//      in this function.
	//      The dependence is basically that when selection is cancelled, I want the
	//      gui to disappear, and that when the window is externally deactivated I
	//      want the selection to reset.
	//      (See WndProc case WM_ACTIVATE for the other side of this dependence.)
	// Adding these for good measure. They seem to solve stuck keys at least some of the time
	//keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
	//keybd_event(VK_SHIFT, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);

	// Adding these for good measure. They seem to solve stuck keys at least some of the time
	//keybd_event(VK_MENU,    0, KEYEVENTF_KEYUP, 0);
	//keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
	//keybd_event(VK_SHIFT,   0, KEYEVENTF_KEYUP, 0);

	KillTimer(gui, /*timerid*/ 1);
	ShowWindow(gui, SW_HIDE); // Yes, call ShowWindow() to hide window...

	if (restore) { 
		// TODO Check that the restore window is not the second most recently
		//      activated window (after our own switcher window), so that I
		//      don't needlessly switch to the window that will naturally
		//      become activated once our switcher window hides
		show(window_restore);
	}

	window_highlighted = NULL;
	window_foregrounded = NULL;
	window_restore = NULL;

	//synckeys();
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
static void redrawgui() {
	// TODO Use 'config.style' 

	#define BACKGROUND   RGB(32, 32, 32) // dark mode?
	#define TEXT_COLOR   RGB(235, 235, 235)
	#define HIGHLIGHT    RGB(76, 194, 255) // Sampled from Windows 11 Alt+Tab
	#define HIGHLIGHT_BG RGB(11, 11, 11) // Sampled from Windows 11 Alt+Tab

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


	i32 i = 0;
	linked_window *window = &windows[0]; // windows[0] is always a top window because it is necessarily the first window of its app in the list
	while (window) {
		linked_window *top1 = find_first(window, false);
		linked_window *top2 = find_first(window_highlighted, false);
		bool app_is_selected = (top1 == top2);

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

		//RECT app_rect = (RECT){};
		//bool app_is_mouseover = ;

		if (!app_is_selected) {
			// Draw only icon, with window background
			DrawIconEx(gdi.context, left, top, window->icon, width, height, 0, window_background, DI_NORMAL);
		} else {
			// Draw selection rectangle and icon, with selection background
			RoundRect(gdi.context, left - SEL_VERT_OFF, top - SEL_HORZ_OFF, right + SEL_VERT_OFF, bottom + SEL_HORZ_OFF, SEL_RADIUS, SEL_RADIUS);
			DrawIconEx(gdi.context, left, top, window->icon, width, height, 0, selection_background, DI_NORMAL);
			
			// Draw app name
			u16 *title;
			if (window->appname.ok) {
				title = window->appname.text;
			} else {
				title = window->filename.text;
			}

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

		// Go to draw next linked window
		if (window = window->next_app) { // Yes, assignment
			i++;
		} else {
			break;
		}
	}
}

static void autorun(bool enabled, string reg_key_name, string launch_args) {

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

static intptr_t LLKeyboardProc(int nCode, intptr_t wparam, intptr_t lparam) {
	if (nCode < HC_ACTION) { // "If code is less than zero, the hook procedure must pass the message to..." blah blah blah read the docs
		return CallNextHookEx(NULL, nCode, wparam, lparam);
	}

	// a) There are situations when my kbd hook is not called, for example when elevated processes consume keyboard events.
	// b) I keep internal state for all pressed keys.
	// This leads to stuck keys when the kbd hook is not called when some key is released.
	// (Ctrl-Shift-Esc is an example, and will leave Shift stuck in my internal keyboard state.)
	// There seems to be no elegant solution to this problem except running as admin.
	// Trust me, I did my research. To get you started: https://www.codeproject.com/Articles/716591/Combining-Raw-Input-and-keyboard-Hook-to-selective
	// My solution is to adopt a sync point to recover from bad ("stuck") state.
	if (!window_highlighted) { // i.e. our switcher is inactive
		synckeys();
	}

	// Info about the keyboard for the low level hook
	KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT *)lparam;
	vkcode key = kbd->vkCode;
	bool key_down = !(kbd->flags & LLKHF_UP);
	bool key_repeat = setkey(key, key_down); // Manually track key repeat using 'keyboard->keys' (see above)

	/*
	if ((key == 160 || key == 162 || key == 164) && !key_repeat) {
		print(L"ctrl %s\talt %s\tshift %s\n", 
			getbit(keyboard, 162) ? L"dn" : L"up",
			getbit(keyboard, 164) ? L"dn" : L"up",
			getbit(keyboard, 160) ? L"dn" : L"up");
	}
	*/
	if (0 && !key_repeat) {
		int count = 0;
		for (int i = 0; i < sizeof keyboard * CHAR_BIT; i++) {
			if (getbit(keyboard, i)) {
				print(L"%i ", i);
				count++;
			}
		}
		if (count > 0) {
			print(L"\n");
		}
	}

	// Typically:
	//  mod1+key1 = Alt+Tab
	//  mod2+key2 = Alt+`   (scancode 0x29)
	// TODO Remove mod2 ? Just have one modifier ?
	bool shift_held  = getkey(VK_LSHIFT) || getkey(VK_RSHIFT);
	print(L"%s", shift_held && !key_repeat ? L"shift\n" : L"");
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

	// NOTE By returning a non-zero value from the hook procedure, the message
	//      is consumed and does not get passed to the target window
	i64 pass_message = 0;
	i64 consume_message = 1;

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
	// [ ] Ctrl+Alt+Tab for "persistent" GUI

	// BUG! Can't use Ctrl+Alt+Tab, because this is the "persistent alt tab" hotkey
	// Ctrl+Alt+Tab (has to be high up since the condition checking conflicts with Alt+Tab (cuz Alt+Tab doesn't check for !ctrl_held)
	// BUG! This is really buggy when passing between Windows' Alt-Tab and ours
	//      Hanging modifiers, missing modifiers...
	//      Passing from Windows' Alt-Tab to ours doesn't even work (oh wait, is that because Ctrl+Alt+Tab is the persistent alt tab hotkey?)
	if (key1_down && mod1_held && ctrl_held) {
		#if 0
		bool enabled = config.switch_apps = !config.switch_apps; // NOTE Atm, I only support a hotkey for toggling app switching, not toggling window switching (cuz why would we?)
		if (!enabled) {
			cancel(, false);
			return pass_message;
		} else {
			// pass-through to Alt+Tab below
		}
		#endif
	}

	// ========================================
	// Activation
	// ========================================
	
	// First Alt+Tab
	if (!window_highlighted && key1_down && mod1_held && config.switch_apps) {
		reinit();
		select_foreground(); // Sets 'window_highlighted'
		// NOTE No return or goto, as the two activations fallthrough to navigation below
	}
	
	// First Alt+`
	if (!window_highlighted && key2_down && mod2_held && config.switch_windows) {
		reinit();
		select_foreground(); // Sets 'window_highlighted'
		select_next(false, shift_held, !config.wrap_bump || !key_repeat); // TODO / BUG Could potentially leave window_highlighted == NULL
		// NOTE No return or goto, as the two activations fallthrough to navigation below
	}

	// ========================================
	// Navigation
	// ========================================

	// Alt+Tab - next app (hold Shift for previous)
	if (window_highlighted && key1_down && mod1_held) {
		select_next(true, shift_held, !config.wrap_bump || !key_repeat); // TODO / BUG Could potentially leave window_highlighted == NULL
		if (config.show_gui_for_apps) {
			resizegui(); // NOTE Must call resizegui after reinit, before redrawgui & before showafter
			redrawgui();
			showafter(gui, config.show_gui_delay);
		}
		if (config.fast_switching_apps) {
			//showhighlighted();
			preview(window_highlighted);
		}

		goto consume_message; // Since I "own" this hotkey system-wide, I consume this message unconditionally
	}

	// Alt+` - next window (hold Shift for previous)
	if (window_highlighted && key2_down && mod2_held) {
		if (window_foregrounded || window_foregrounded == window_highlighted) {
			select_next(false, shift_held, !config.wrap_bump || !key_repeat); // TODO / BUG Could potentially leave window_highlighted == NULL
			//print(L"Alt+`ing after having alt+`ed\n");
		} else {
			//print(L"Alt+`ing after having alt+tab'ed\n");
		}

		if (config.show_gui_for_windows) {
			resizegui(); // NOTE Must call resizegui after reinit, before redrawgui & before showafter
			redrawgui();
			showafter(gui, config.show_gui_delay);
		}
		if (config.fast_switching_windows) {
			showhighlighted();
			//preview(window_highlighted);
		}

		goto consume_message; // Since I "own" this hotkey system-wide, I consume this message unconditionally
	}

	// Alt+Arrows. People ask for this, so...
	if (window_highlighted && (next_down || prev_down) && mod1_held) { // window_highlighted: You can't *start* window switching with Alt+Arrows, but you can navigate the switcher with arrows when switching is active
		select_next(true, prev_down, !config.wrap_bump || !key_repeat);
		if (config.fast_switching_apps) {
			showhighlighted();
			//peek(window_highlighted->hwnd); // TODO Doesn't work yet. See config.fast_switching and peek
		}
		if (config.show_gui_for_apps) {
			// Update selection highlight
			redrawgui();
		}

		goto consume_message; // Since our swicher is active, I "own" this hotkey, so I consume this message
	}

	// ========================================
	// Deactivation
	// ========================================

	// Alt keyup - switch to selected window
	// BUG! Aha! Lemme tell yalittle secret: Our low level hook doesn't observe "Alt keydown" because Alt can be pressed down for
	//      so many purposes on system level. I'm only interested in Alt+Tab, which means that I only really check for "Tab keydown",
	//      and then transparently check if Alt was already down when Tab was pressed.
	//      This means that I let all "Alt keydown"s pass through our hook, even the "Alt keydown" that the user pressed right before
	//      pressing Tab, which is sort of "ours", but I passed it through anyways (can't consume it after-the-fact).
	//      The result is that there is a hanging "Alt down" that got passed to the foreground app which needs an "Alt keyup", or
	//      the Alt key gets stuck. Thus, I always pass through "Alt keyup"s, even if I used it for some functionality.
	//      In short:
	//      1) Our app never hooks Alt keydowns
	//      2) Our app hooks Alt keyups, but always passes it through on pain of getting a stuck Alt key because of point 1
	if (window_highlighted && (mod1_up || mod2_up)) {
		showhighlighted();
		cancel(false);
		goto pass_message; // See note above on "Alt keyup" on why I don't consume this message
	}
	// Alt+Enter - switch to selected window
	if (window_highlighted && (mod1_held || mod2_held) && enter_down) {
		showhighlighted();
		cancel(false);
		goto consume_message; // Since our swicher is active, I "own" this hotkey, so I consume this message
	}
	// Alt+Esc - cancel switching and restore initial window
	if (window_highlighted && (mod1_held || mod2_held) && esc_down) { // BUG There's a bug here if mod1 and mod2 aren't both the same key. In that case I'll still pass through the key event, even though I should own it since it's not Alt+Esc
		cancel(true);
		goto consume_message;
	}

	// ========================================
	// Extended window management
	// ========================================
	
	// Alt+F4 - quit cmdtab
	if (window_highlighted && ((mod1_held || mod2_held) && keyF4_down)) {
		close(gui);
		goto consume_message;
	}
	// Alt+Q - close all windows of selected application
	if (window_highlighted && ((mod1_held || mod2_held) && keyQ_down)) {
		print(L"Q\n");
		goto consume_message;
	}
	// Alt+W or Alt+Delete - close selected window
	if (window_highlighted && ((mod1_held || mod2_held) && (keyW_down || delete_down))) {
		close(window_highlighted->hwnd);
		reinit();
		resizegui(); // NOTE Must call resizegui after reinit, before redrawgui & before showafter
		redrawgui();
		goto consume_message;
	}
	// Alt+M - minimize selected window
	if (window_highlighted && ((mod1_held || mod2_held) && keyM_down)) {
		minimize(window_highlighted->hwnd);
		goto consume_message;
	}
	// Alt+B - display app name and window class to user, to add in blacklist
	if (window_highlighted && ((mod1_held || mod2_held) && keyB_down)) {
		snitch();
		goto consume_message;
	}
	// Input sink - consume keystrokes when activated
	if (window_highlighted && (mod1_held || mod2_held)) {
		goto consume_message;
	}

	pass_message:
		return CallNextHookEx(NULL, nCode, wparam, lparam);
	consume_message:
		return consume_message;
}
static intptr_t WndProc(handle hwnd, unsigned message, intptr_t wparam, intptr_t lparam) {
	switch (message) {
		case WM_ERASEBKGND: {
			return 1;
		}
		case WM_ACTIVATE: {
			// Cancel on mouse clicking outside window
			if (wparam == WA_INACTIVE) {
				//print(L"gui lost focus\n");
				//cancel(false); // NOTE See comments in cancel
			} else {
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
			short x = ((int)(short)LOWORD(lparam));
			short y = ((int)(short)HIWORD(lparam));
			mouse_x = x;
			mouse_y = y;
			//print(L"x%i y%i\n", x, y);
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
static void WinEventProc(handle hook, u32 event, handle hwnd, i32 object_id, i32 child_id, u32 thread_id, u32 timestamp) 
{
	if (event == EVENT_SYSTEM_FOREGROUND) {
		print(L"FOREGROUND %i, %i: ", object_id, child_id);
		print_hwnd(hwnd, true);
		PlaySoundW(L"C:\\Windows\\Media\\Speech Misrecognition.wav", NULL, SND_FILENAME | SND_ASYNC);
	}
	if (event == EVENT_OBJECT_DESTROY && hwnd && GetWindow(hwnd, GW_OWNER)) {
		print(L"DESTROY %i, %i: ", object_id, child_id);
		print_hwnd(hwnd, true);
		PlaySoundW(L"C:\\Windows\\Media\\Windows Information Bar.wav", NULL, SND_FILENAME | SND_ASYNC);
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {

	// Prompt about autorun (unless I got autorun arg)
	string autorun_arg = string(L"--autorun");
	string registry_key = string(L"cmdtab");
	if (wcscmp(pCmdLine, autorun_arg.text)) { // lstrcmpW, CompareStringW
		bool autorun_enable = ask(L"Start cmdtab automatically?\nRelaunch cmdtab.exe to change your mind.");
		autorun(autorun_enable, registry_key, autorun_arg);
	}

	// Quit if another instance of cmdtab is already running
	handle hMutex = CreateMutexW(NULL, true, L"cmdtab_mutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		int result = MessageBoxW(NULL, L"cmdtab is already running.", L"cmdtab", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
		return result; // 0
	}

	// Initialize runtime-dependent settings (dependent on user's current kbd layout)
	config.key2 = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX);

	// Install (low level) keyboard hook
	handle hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, (HOOKPROC)LLKeyboardProc, NULL, 0);
	// Install window event hook for one event: foreground window changes. Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20130930-00/?p=3083
	//handle hWinEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
	handle hWinEventHook2 = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
	
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
	UnhookWinEvent(hWinEventHook2);
	//CloseHandle(hKeyboardHook);
	if (hMutex) {
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
	}

	print(L"done\n");
	_CrtDumpMemoryLeaks();
	return (int)msg.wParam;
}
