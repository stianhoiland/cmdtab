//#define WIN32_LEAN_AND_MEAN
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

#ifdef _DEBUG
#define debug() __debugbreak() // Or DebugBreak in debugapi.h
#elif
#define debug() 
#endif

#define countof(a) (ptrdiff_t)(sizeof(a) / sizeof(*a))

#define false 0
#define true 1
#define null NULL

typedef unsigned char        u8;
typedef wchar_t             u16; // "Microsoft implements wchar_t as a two-byte unsigned value."
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed short        i16;
typedef signed int          i32;
typedef signed long long    i64;
typedef signed int         bool;
typedef ptrdiff_t          size;
//typedef size_t               uz;
//typedef struct {int _;} *handle;
typedef void *           handle;
typedef struct tagRECT     rect; // WE DOES WINDOWS SCREAM AT ME
typedef unsigned char    vkcode;

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


/*
static struct strings {
	u16 heap[1<<24 / sizeof(u16)]; // 16mb
	const struct arena {
		u16 *start;
		u16 *end;
	} arena;
} strings = {
	//.heap = {0}, // not necessary cuz static
	.arena = {
		strings.heap,
		strings.heap + sizeof strings.heap,
	}
};

#define allocate(t, n, a) (t *)strings_allocate(a, n, sizeof(t), _Alignof(t))
static void *strings_allocate(struct arena *mem, size count, size sz, size alignment)
{
	size padding = -(uintptr_t)mem->start & (alignment - 1);
	assert(count < (mem->end - mem->start - padding) / sz);
	void *start = mem->start + padding;
	mem->start += padding + count*sz;
	return memset(start, 0, count*sz);
}

typedef struct string {
	u16 *data;
	size size; // size in bytes
} string;

#define string(s1) {s1,sizeof s1/sizeof *s1}
#define strlen(s1) (sizeof *s1.data*s1.size)

static string strnew(str src)
{
	str dst;
	dst.data = memcpy(allocate(byte, src.size, arena), src.data, src.size);
	dst.size = src.size;
	return dst;
}

static string concat(string dst, string src, memory *mem)
{
	if (!dst.data || mem->start != (byte *)(dst.data + dst.length)) { // is this cast necessary?
		dst = copystr(dst, mem);
	}
	dst.length += copystr(src, mem).length;
	return dst;
}




#define strnew(s1) (struct str){s1,sizeof *s1.data*wcslen(s1),1}
#define strstatic(s1) {s1,sizeof s1/sizeof *s1,1}
#define strlen(s1) (sizeof *s1.data*s1.size)

size strset(str *s1, str *s2)
{
	if (!s2.size) s2.size = wcslen(s2.data);
	memcpy(s1.data, s2.data, s1.size);
	return s1.size;
}
str strcpy(str *s1)
{
	str s2;
	s2.size = strset(&s2, s1);
	return s2;
}
size strcat(str *s1, str *s2)
{

}
*/


typedef struct string string;
//typedef struct app app_t;
//typedef struct gdi gdi_t;
//typedef struct identifier identifier;
//typedef struct config config_t;

#define string(str) { str, countof(str)-1, true } // TODO: -1 to remove NUL-terminator?

struct string {
	u16 text[(MAX_PATH*2)];
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

//==============================================================================
// App state
//==============================================================================

// Go from supporting max. 256 windows @ 690kb to supporting max. 8192 windows @ 340kb 
//  (49% memory decrease, 3200% capacity increase),
//  but sectioned into max. 128 apps, each with max. 64 windows
#define APPS_MAX 128
#define WNDS_MAX  64
#define QUEUE_MAX 64
#define BLACKLIST_MAX 32

static const string UWPHostExecutable = string(L"ApplicationFrameHost.exe");
static const string UWPHostClass      = string(L"ApplicationFrameWindow");
static const string UWPHostedClass    = string(L"Windows.UI.Core.CoreWindow");
static const string UWPHostedPath     = string(L"C:\\Program Files\\WindowsApps");

static struct app {                // Filtered apps that are displayed in switcher
	string filepath;               //     Full exe path
	string appname;                //     Product description/name, or filename w/o extension as fallback
	handle icon;                   //     Big icon
	handle windows[WNDS_MAX];      //     Filtered windows that can be switched to
	size windows_count;            //     Number of elements in 'windows' array
} apps[APPS_MAX];                  //

static size apps_count;            // Number of elements in 'apps' array
static struct app *selected_app;   // The app for the 'selected_window'
static handle *selected_window;    // Currently selected window in switcher. Non-NULL indicates switcher is active
static handle foregrounded_window; // Last window switched-to by switcher // TODO Remove?
static handle restore_window;      // Whatever window was foreground when switcher showed. Does not need to be in filtered 'apps' array
static handle queue[QUEUE_MAX];    // Defer window activations while the switcher is active because window activations change app and window order in switcher
static size queue_count;           // The activation queue is committed when the switcher deactivates or if the queue is about to overflow
static handle gui;                 // Handle for main window (switcher)
static handle gdi_context;         // Off-screen bitmap used for double-buffering
static handle gdi_bitmap;          // 
static rect gdi_rect;              // 
static u8 keyboard[32];            // 256 bits for tracking key repeat when using low-level keyboard hook
static i32 mouse_x, mouse_y;

static void _showgui(handle, u32, u64, u32); // TimerProc used by showgui
static bool _addwindow(handle, i64);         // EnumWindowsProc used by addwindow
static bool _getuwphosted(handle, i64);      // EnumChildProc used by getuwp

struct hotkey {
	enum modifier {
		CTRL   = VK_CONTROL,
		LCTRL  = VK_LCONTROL,
		RCTRL  = VK_RCONTROL,
		ALT    = VK_MENU,
		LALT   = VK_LMENU,
		RALT   = VK_RMENU,
		SHIFT  = VK_SHIFT,
		LSHIFT = VK_LSHIFT,
		RSHIFT = VK_RSHIFT,
	} mod;
	vkcode key;
};

static struct config {
	// Hotkeys
	struct hotkey hotkey_apps;
	struct hotkey hotkey_windows;
	// Behavior
	bool switch_apps;
	bool switch_windows;
	bool fast_switching_apps; // TODO "perviewing" not implemented yet. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
	bool fast_switching_windows; // TODO "perviewing" not implemented yet. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
	bool show_gui_for_apps;
	bool show_gui_for_windows;
	u32  show_gui_delay;
	bool wrap_bump;
	bool ignore_minimized; // TODO Atm, 'ignore_minimized' affects both Alt-Tilde/Backquote and Alt-Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.) TODO Should this be called 'restore_minimized'?
	bool restore_on_cancel;
	// Appearance
	u32 gui_height;
	u32 gui_horz_margin;
	u32 gui_vert_margin;
	u32 icon_width;
	u32 icon_horz_padding;
	// Blacklist
	struct identifier {
		u16 *filename; // Filename without file extension, ex. "explorer"
		u16 *class;  // Window class name
	} blacklist[BLACKLIST_MAX];
} config = {
	
	// The default key2 is scan code 0x29 (hex) / 41 (decimal)
	// This is the key that is physically below Esc, left of 1 and above Tab
	//
	// WARNING!
	// VK_OEM_3 IS NOT SCAN CODE 0x29 FOR ALL KEYBOARD LAYOUTS.
	// YOU MUST CALL THE FOLLOWING DURING RUNTIME INITIALIZATION:
	// `.key2 = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX)`
	
	// Hotkeys
	.hotkey_apps = { LALT, VK_TAB },
	.hotkey_windows = { LALT, VK_OEM_3 }, // Must be updated during runtime. See note above.
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
		  { null,                    L"ApplicationFrameWindow" },
		//{ L"ApplicationFrameHost", null },
		//{ L"ApplicationFrameHost", L"ApplicationFrameWindow" },
		//{ null,                    L"Windows.UI.Core.CoreWindow" },
		//{ null,                    L"Xaml_WindowedPopupClass" },
		  { L"SearchHost",           L"Windows.UI.Core.CoreWindow" },
		//{ L"explorer",             L"ApplicationFrameWindow" },
		//{ L"explorer",             L"ApplicationManager_DesktopShellWindow" },
		//{ L"TextInputHost",        null },
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
static void error(u16 *fmt, ...)
{
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	OutputDebugStringW(buffer);
	MessageBoxW(gui, buffer, L"cmdtab", MB_OK | MB_ICONERROR | MB_TASKMODAL);
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
static handle getuwphosted(handle hwnd)
{
	string class = getclass(hwnd);
	if (!equals(&class, &UWPHostClass)) {
		return hwnd;
	}
	handle hwnd2 = hwnd;
	EnumChildWindows(hwnd, _getuwphosted, &hwnd2); // _getuwp returns hosted window, if any
	return hwnd2;
}
static bool _getuwphosted(handle hwnd, i64 lparam)
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
static handle getuwphost(handle hwnd)
{
	string class = getclass(hwnd);
	if (!equals(&class, &UWPHostedClass)) {
		return hwnd;
	}
	return GetAncestor(hwnd, GA_PARENT);
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
		print(L"WARNING couldn't open process with pid: %u\n", pid);
		return path;
	}
	path.length = countof(path.text);
	path.ok = QueryFullProcessImageNameW(process, 0, path.text, &path.length); // NOTE Can use GetLongPathName() if getting shortened paths
	CloseHandle(process);
	if (!path.ok) {
		print(L"WARNING couldn't get exe path for process with pid: %u\n", pid);
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

	if (FAILED(CoInitialize(null))) { // In case we use COM
		return null;
	}
	SHFILEINFOW info = {0};
	if (!SHGetFileInfoW(filepath, 0, &info, sizeof info, SHGFI_SYSICONINDEX)) { // 2nd arg: -1, 0 or FILE_ATTRIBUTE_NORMAL
		debug(); // SHGetFileInfoW() has special return with SHGFI_SYSICONINDEX flag, which I think should never fail (docs unclear?)
		return null;
	}
	IImageList *list = {0};
	if (FAILED(SHGetImageList(SHIL_JUMBO, &IID_IImageList, (void **)&list))) {
		return null;
	}
	HICON icon = {0};
	IImageList_GetIcon(list, info.iIcon, ILD_TRANSPARENT, &icon);
	IImageList_Release(list);
	return icon; // Caller is responsible for calling DestroyIcon 
}

static bool getdarkmode() {
	DWORD value;
	DWORD size = sizeof value;
	if (ERROR_SUCCESS == RegGetValueW(
			HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUsesLightTheme", 
			RRF_RT_DWORD, null, 
			&value, &size)) {
		return !value; // Value is true for light mode
	} else {
		return true; // Default to dark mode
	}
}
static u32 getaccentcolor() {
	DWORD value;
	DWORD size = sizeof value;
	if (ERROR_SUCCESS == RegGetValueW(
			HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM\\", L"AccentColor",
			RRF_RT_DWORD, null, 
			&value, &size)) {
		return value;
	} else {
		//       AABBGGRR
		return 0xFFFFC24C; // Default to default aqua color, sampled from Windows 11 Alt-Tab
	}
}

static bool getkey(vkcode key)
{
	return (bool)(GetAsyncKeyState(key) & 0x8000);
}
static bool setkey(vkcode key, bool down)
{
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
#define bitindex(b)   ((b) / CHAR_BIT)
#define bitmask(b)    (1 << ((b) % CHAR_BIT))
#define setbit1(a, b) ((a)[bitindex(b)] |=  bitmask(b))
#define setbit0(a, b) ((a)[bitindex(b)] &= ~bitmask(b))
#define getbit(a, b)  ((a)[bitindex(b)] &   bitmask(b))

	bool wasdown = getbit(keyboard, key);
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
		//setkey(i, GetAsyncKeyState(i) & 0x8000);
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
		string filename = getfilename(filepath.text, false);
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

		print(L"%p { %s, %s }%s \"%.32s\"", hwnd, filename.text, class.text, ws_iconic, title.text);
		print(L" (%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s)\n",
			ws_visible, ws_child, ws_disabled, ws_popup, ws_popupwindow, ws_tiled, ws_tiledwindow, ws_overlapped, ws_overlappedwindow, ws_dlgframe,
			ex_toolwindow, ex_appwindow, ex_dlgmodalframe, ex_layered, ex_noactivate, ex_palettewindow, ex_overlappedwindow);
	} else {
		print(L"null hwnd\n");
	}
}
static void printapps()
{
	for (int i = 0; i < apps_count; i++) {
		print(L"%i ", i);
		struct app *app = &apps[i];
		printhwnd(app->windows[0]);
		for (int j = 1; j < app->windows_count; j++) {
			print(L"  %i ", j);
			printhwnd(app->windows[j]);
		}
	}
}
static void printbits(uintptr_t bits)
{
	for (int i = 0; i < sizeof bits * CHAR_BIT; i++) {
		print(L"%u", (bits >> i) & 1);
		if ((i+1) % 8 == 0) {
			print(L" ");
		}
	}
	print(L"\n");
}

static void hide(handle hwnd)
{
	// WARNING Can't use this because we filter out hidden windows
	//static const int SW_HIDE = 0;
	//ShowWindow(hwnd, SW_HIDE);
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
static void peek(handle hwnd)
{
	//typedef HRESULT (__stdcall *DwmpActivateLivePreview)(bool peekOn, handle hPeekWindow, handle hTopmostWindow, u32 peekType1or3, i64 newForWin10);
	//DwmpActivateLivePreview(true, hwnd, gui, 3, 0);
	// NOT IMPLEMENTED YET, FALLBACK TO SHOW
	void show(handle hwnd);
	show(hwnd);
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
	SendInput(1, &(INPUT){.type = INPUT_KEYBOARD, .ki.wVk = VK_MENU, .ki.dwFlags = KEYEVENTF_KEYUP}, sizeof(INPUT));
}
static void show(handle hwnd) // TODO return bool
{
	if (GetForegroundWindow() != hwnd) {
		if (IsIconic(hwnd)) {
			ShowWindow(hwnd, SW_RESTORE);
		} else {
			ShowWindow(hwnd, SW_SHOWNA);
		}
		altup();//hack();
		if (SetForegroundWindow(hwnd)) {
			foregrounded_window = hwnd; // This one is a little sneaky, hiding in here...
			print(L"switch to ");
			printhwnd(hwnd);
		} else {
			error(L"ERROR couldn't switch to ");
			printhwnd(hwnd);
			//cancel(false);
		}
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

static void mark()
{
	// Flip all window handles' first bit from 0 to 1, invalidating them
	// Depends on all window handles having unused first bit (i.e. at least 2-byte aligned), which they are (On My ComputerTM)
	for (int i = 0; i < apps_count; i++) {
		for (int j = 1; j < apps[i].windows_count; j++) {
			(uintptr_t)apps[i].windows[j] |= (uintptr_t)(1 << 0);
		}
	}
}
static bool markcompare(handle h1, handle h2)
{
	if ((uintptr_t)h1 & (uintptr_t)(1 << 0)) {
		// Compare two handles, ignoring flipped first bit
		h1 = (uintptr_t)h1 |= (uintptr_t)(0 << 0);
		h2 = (uintptr_t)h2 |= (uintptr_t)(0 << 0);
		return h1 == h2;
		return ((uintptr_t)h1 & (uintptr_t)h2) & (uintptr_t)(0 << 0);
	}
	return false;
}
static void sweep()
{
	for (int i = 0; i < apps_count; i++) {
		for (int j = 1; j < apps[i].windows_count; j++) {
			if ((uintptr_t)apps[i].windows[j] & (uintptr_t)(1 << 0)) {
				print(L"invalidated ");
				printhwnd(apps[i].windows[j]);
				//printbits(apps[i].windows[j]);
			}
		}
	}
}

/*
static bool ignored(handle hwnd)
{
	// Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
	if (IsWindowVisible(hwnd) == false) {
		return true;
	}
	int cloaked = 0;
	DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked); // Windows 8
	if (cloaked) {
		// omg exceptions upon corner cases upon...
		//string class = getclass(hwnd);
		//if (!equals(&class, &UWPHostClass)) {
			return true;
		//}
	}
	// "A tool window [i.e. WS_EX_TOOLWINDOW] does not appear in the taskbar or
	// in the dialog that appears when the user presses ALT-TAB." (docs)
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
	// 2) {name,null}  only name is specified and must match
	// 3) {null,class} only class is specified and must match
	// 4) {null,null}  terminates array
	for (size i = 0; i < countof(config.blacklist); i++) {
		struct identifier ignored = config.blacklist[i];
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
*/
static bool addwindow(handle hwnd)
{
	// In broad strokes:
	// 1. Validate hwnd
	// 2. If hwnd is valid, but is already added, move hwnd to top of app's window list (i.e. treat as window activation)
	// 3. If hwnd is valid, and is not already added, append hwnd to app's window list (i.e. treat as iterating windows in Z-order)

	// 1. Get hosted window in case of UWP host
	string class = getclass(hwnd);
	if (equals(&class, &UWPHostClass)) {
		EnumChildWindows(hwnd, _getuwphosted, &hwnd); // _getuwphosted sets passed param to hosted window, if any
		class = getclass(hwnd);
	}
	// 2. Check window for basic eligibility in Alt-Tab
	// Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
	if (!IsWindowVisible(hwnd)) {
		goto ignored;
	}
	int cloaked = 0;
	DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked); // Windows 8
	if (cloaked) {
		// omg exceptions upon corner cases upon...
		//string class = getclass(hwnd);
		//if (!equals(&class, &UWPHostClass)) {
			goto ignored;
		//}
	}
	// "A tool window [i.e. WS_EX_TOOLWINDOW] does not appear in the taskbar or
	// in the dialog that appears when the user presses ALT-TAB." (docs)
	if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
		goto ignored;
	}
	if (config.ignore_minimized && IsIconic(hwnd)) { // TODO Hmm, rework or remove this feature
		goto ignored;
	}
	// 3. Get module filepath, abort if failed (most likely access denied)
	string filepath = getfilepath(hwnd);
	if (!filepath.ok) {
		return false; // Don't have to print error, since getfilepath already does that
	}
	// 4. Check window against user-defined blacklist
	#define wcseq(str1, str2) (wcscmp(str1, str2) == 0)
	// Iterate blacklist. I allow three kinds of entries in the black list:
	// 1) {name,class} both name and class are specified and both must match
	// 2) {name,null}  only name is specified and must match
	// 3) {null,class} only class is specified and must match
	// 4) {null,null}  terminates array
	string filename = getfilename(filepath.text, false);
	for (size i = 0; i < countof(config.blacklist); i++) {
		struct identifier ignored = config.blacklist[i];
		if (ignored.filename && ignored.class) {
			if (wcseq(ignored.filename, filename.text) &&
				wcseq(ignored.class, class.text)) {
				goto blacklisted;
			}
		} else if (ignored.filename && !ignored.class && wcseq(ignored.filename, filename.text)) {
			goto blacklisted;
		} else if (!ignored.filename && ignored.class && wcseq(ignored.class, class.text)) {
			goto blacklisted;
		} else if (!ignored.filename && !ignored.class) {
			break;
		}
	}
	// 5. Lookup app with same module filepath in app list
	struct app *app = null;
	for (int i = 0; i < apps_count; i++) {
		if (equals(&filepath, &apps[i].filepath)) {
			app = &apps[i];
			break;
		}
	}
	if (app) { // Don't have to check if window already tracked if app didn't exist
		
		/*
		for (int i = 0; i < app->windows_count; i++) {
			if (markcompare(app->windows[i], hwnd)) {
				// remove mark
				//app->windows[i] = hwnd;
				(uintptr_t)apps->windows[i] |= (uintptr_t)(0 << 0);
				return;
			}
		}
		*/
		
		// Lookup window in app's window list
		handle *window = null;
		for (int i = 0; i < app->windows_count; i++) {
			if (app->windows[i] == hwnd) {
				window = &app->windows[i];
				break;
			}
		}
		if (window) {
			// 6. If app and window already tracked, put window first in app's window list
			//    and app first in app list, and return early

			// Lots of noise here. Don't worry, it's just for detailed printing. The business logic fits on 10 simple lines

			print(L"activated ");
			printhwnd(hwnd);

			if (window > &app->windows[0]) { // Don't shift if already first
				print(L"  window list before reorder\n");
				for (int i = 0; i < app->windows_count; i++) {
					print(L"    %i", i);
					printhwnd(app->windows[i]);
				}

				{
					// Shift other windows down to position of 'window' and then put 'window' first
					memmove(&app->windows[1], &app->windows[0], sizeof app->windows[0] * (window-app->windows)); // i.e. index
					app->windows[0] = hwnd;
				}

				print(L"  window list after reorder\n");
				for (int i = 0; i < app->windows_count; i++) {
					print(L"    %i", i);
					printhwnd(app->windows[i]);
				}
			} else {
				print(L"  already first in window list\n");
			}

			if (app > &apps[0]) { // Don't shift if already first

				print(L"  app list before reorder\n");
				for (int i = 0; i < apps_count; i++) {
					print(L"    %i", i);
					printhwnd(apps[i].windows[0]);
				}

				{
					// Shift other apps down to position of 'app' and then put 'app' first
					struct app save = *app; // This is a 2.6kb copy
					memmove(&apps[1], &apps[0], sizeof apps[0] * (app-apps)); // i.e. index
					apps[0] = save;
				}

				print(L"  app list after reorder\n");
				for (int i = 0; i < apps_count; i++) {
					print(L"    %i", i);
					printhwnd(apps[i].windows[0]);
				}
			} else {
				print(L"  already first in app list\n");
			}

			return true; // Early return, do not add already tracked windows
		}
	} else { // if (!app) {
		// 7. If app not already tracked, create new app entry
		//    NOTE: Because of lazy memory management, must overwrite all fields and
		//          free icon in old app entry
		if (apps_count >= countof(apps)) {
			error(L"ERROR reached max. apps\n");
			return false;
		}
		string appname = getappname(filepath.text);
		if (!appname.ok) {
			appname = filename; // filename from earlier when checking blacklist
		}
		app = &apps[apps_count++]; // Since I reset by truncating apps_count, make sure to overwrite all fields when reusing array slots
		if (app->icon) DestroyIcon(app->icon); // In fact, I also have to do a little bit of necessary cleanup lazily
		app->filepath = filepath;
		app->appname = appname;
		app->icon = geticon(filepath.text);
		app->windows_count = 0; // Same truncation trick here (but hwnds are not structs, so no fields to overwrite)
	}
	if (app->windows_count >= countof(app->windows)) {
		error(L"ERROR reached max. windows for app\n");
		return false;
	}
	// 8. Add window to app's window list
	app->windows[app->windows_count++] = hwnd;
	return true;

ignored:
	print(L"ignored ");
	printhwnd(hwnd);
	return false;
blacklisted:
	print(L"blacklisted ");
	printhwnd(hwnd);
	return false;
}
static void resetwindows()
{
	// Rebuild the whole window list and measure how long it takes

	i64 freq, start, end, diff;
	QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
	QueryPerformanceCounter((LARGE_INTEGER *)&start);
	{
		//mark();
		apps_count = 0;
		EnumWindows(_addwindow, 0); // _addwindow calls addwindow with every top-level window enumerated by EnumWindows 
		//print(L"before, bit 0 = 0\n");
		//sweep();
		//print(L"after mark, bit 0 = 1\n");
		//sweep();
	}
	QueryPerformanceCounter((LARGE_INTEGER *)&end);
	diff = ((end - start) * 1000) / freq;

	print(L"elapsed %llums\n", diff);
}
static bool _addwindow(handle hwnd, i64 lparam)
{
	// EnumWindowsProc
	// Call addwindow with every top-level window enumerated by EnumWindows
	addwindow(hwnd);
	return true; // Return true to continue EnumWindows
}
static void prunewindows()
{

}
static void flushqueue() {
	print(L"flush queue\n");
	for (int i = 0; i < queue_count; i++) {
		addwindow(queue[i]);
	}
	queue_count = 0;
}
static void cancel(bool restore) 
{
	print(L"cancel\n");
	KillTimer(gui, /*timerid*/ 1);
	ShowWindow(gui, SW_HIDE); // Yes, call ShowWindow() to hide window...
	if (restore) { 
		// TODO Check that the restore window is not the second most recently
		//      activated window (after my own switcher window), so that I
		//      don't needlessly switch to the window that will naturally
		//      become activated once my switcher window hides
		show(restore_window);
	}
	// Reset selection
	selected_app = null;
	selected_window = null;
	foregrounded_window = null; // See show for the initialization of this one
	restore_window = null;
	// Can commit queue now
	flushqueue();
}
static void selectfirst() {
	if (apps_count > 0) {
		selected_app = &apps[0];
		selected_window = &apps[0].windows[0];
		//foregrounded_window -> See show for the initialization of this one
		restore_window = GetForegroundWindow();
	} else {
		print(L"nothing to select\n");
	}
}
static void selectnext(bool applevel, bool reverse, bool wrap)
{
	if (!selected_app || !selected_window) {
		debug();
	}
	if (applevel) {
		struct app *first_app = &apps[0];
		struct app  *last_app = &apps[apps_count-1];
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
		selected_window = &selected_app->windows[0];
	} else {
		struct app *first_window = &selected_app->windows[0];
		struct app  *last_window = &selected_app->windows[selected_app->windows_count-1];
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
	if (!EqualRect(&gdi_rect, &resized)) {
		handle context = GetDC(gui);
		DeleteObject(gdi_bitmap);
		DeleteDC(gdi_context);
		gdi_rect = resized;
		gdi_context = CreateCompatibleDC(context);
		gdi_bitmap = CreateCompatibleBitmap(context, gdi_rect.right - gdi_rect.left, gdi_rect.bottom - gdi_rect.top);
		handle oldbitmap = (handle)SelectObject(gdi_context, gdi_bitmap);
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
	if (window_background == null) {
		window_background = CreateSolidBrush(BACKGROUND);
	}
	if (selection_background == null) {
		selection_background = CreateSolidBrush(HIGHLIGHT_BG);
	}
	//if (selection_outline == null) {
		selection_outline = CreatePen(PS_SOLID, SEL_OUTLINE, (getaccentcolor() & 0x00FFFFFF)); //HIGHLIGHT); // TODO Leak?
	//}

	RECT rect = {0};
	GetClientRect(gui, &rect);

	// Invalidate & draw window background
	RedrawWindow(gui, null, null, RDW_INVALIDATE | RDW_ERASE);
	FillRect(gdi_context, &rect, window_background);

	// Select pen & brush for RoundRect() (used to draw selection rectangle)
	HPEN old_pen = (HPEN)SelectObject(gdi_context, selection_outline);
	HBRUSH old_brush = (HBRUSH)SelectObject(gdi_context, selection_background);

	// Select text font, color & background for DrawTextW() (used to draw app name)
	HFONT old_font = (HFONT)SelectObject(gdi_context, GetStockObject(DEFAULT_GUI_FONT));
	SetTextColor(gdi_context, TEXT_COLOR);
	SetBkMode(gdi_context, TRANSPARENT);

	// This is how GDI handles memory?
	//DeleteObject(old_pen);
	//DeleteObject(old_brush);
	//DeleteObject(old_font);

	for (int i = 0; i < apps_count; i++) {
		struct app *app = &apps[i];

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
			DrawIconEx(gdi_context, left, top, app->icon, width, height, 0, window_background, DI_NORMAL);
		} else {
			// Draw selection rectangle and icon, with selection background
			RoundRect(gdi_context, left - SEL_VERT_OFF, top - SEL_HORZ_OFF, right + SEL_VERT_OFF, bottom + SEL_HORZ_OFF, SEL_RADIUS, SEL_RADIUS);
			DrawIconEx(gdi_context, left, top, app->icon, width, height, 0, selection_background, DI_NORMAL);
			
			// Draw app name
			u16 *title = app->appname.text;

			// Measure text width
			RECT test_rect = {0};
			DrawTextW(gdi_context, title, -1, &test_rect, DT_CALCRECT | DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
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

			///* dbg */ FillRect(gdi_context, &text_rect, CreateSolidBrush(RGB(255, 0, 0))); // Check the size of the text box
			DrawTextW(gdi_context, title, -1, &text_rect, DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
		}
	}
}

static void autorun(bool enabled, u16 *keyname, u16 *args)
{
	string filepath;
	string target;
	filepath.length = GetModuleFileNameW(null, filepath.text, countof(filepath.text)-1); // Get filepath of current module
	StringCchPrintfW(target.text, countof(target.text)-1, L"\"%s\" %s", filepath.text, args);
	target.length = wcslen(&target.text);
	handle regkey;
	i32 success; // BUG Not checking 'success' below
	if (enabled) {
		success = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, null, 0, KEY_SET_VALUE, null, &regkey, null);
		success = RegSetValueExW(regkey, keyname, 0, REG_SZ, target.text, target.length * sizeof *target.text);
		success = RegCloseKey(regkey);
	} else {
		success = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &regkey);
		success = RegDeleteValueW(regkey, keyname);
		success = RegCloseKey(regkey);
	}
}

static  i64 KeyboardProc(int code, u64 wparam, i64 lparam)
{
	if (code < 0) { // "If code is less than zero, the hook procedure must pass the message to..." blah blah blah read the docs
		return CallNextHookEx(null, code, wparam, lparam);
	}
	
	KBDLLHOOKSTRUCT *kbd = lparam;
	vkcode key_code = (vkcode)kbd->vkCode;
	bool key_down = !(bool)(kbd->flags & LLKHF_UP);

	if (!selected_window) {

		bool key_repeat = setkey(key_code, key_down); // Manually track key repeat
		bool shift_held = getkey(VK_LSHIFT) || getkey(VK_RSHIFT) || getkey(VK_SHIFT);

		// ========================================
		// Activation
		// ========================================

		bool key1_down = key_code == config.hotkey_apps.key && key_down;
		bool key2_down = key_code == config.hotkey_windows.key && key_down;
		bool mod1_held = getkey(config.hotkey_apps.mod);
		bool mod2_held = getkey(config.hotkey_windows.mod);

		// First Alt-Tab
		if (key1_down && mod1_held && config.switch_apps) {
			selectfirst(); // Initialize selection
			if (*selected_window == GetForegroundWindow()) { // Don't select next when on blank desktop
				selectnext(true, shift_held, !config.wrap_bump || !key_repeat);
			}
			if (config.show_gui_for_apps) {
				resizegui(); // NOTE Must call resizegui after activate, before redrawgui & before showgui
				redrawgui();
				showgui(config.show_gui_delay);
			}
			if (config.fast_switching_apps) {
				peek(*selected_window);
			}
			// Since I "own" this hotkey system-wide, I consume this message unconditionally
			// This will consume the key1_down event. Windows will not know that key1 was pressed
			goto consume_message;
		}
		// First Alt-Tilde/Backquote
		if (key2_down && mod2_held && config.switch_windows) {
			selectfirst(); // Initialize selection
			selectnext(false, shift_held, !config.wrap_bump || !key_repeat);
			if (config.show_gui_for_windows) {
				resizegui(); // NOTE Must call resizegui after activate, before redrawgui & before showgui
				redrawgui();
				showgui(config.show_gui_delay);
			}
			if (config.fast_switching_windows) {
				peek(*selected_window);
			}
			// Since I "own" this hotkey system-wide, I consume this message unconditionally
			// This will consume the key2_down event. Windows will not know that key2 was pressed
			goto consume_message;
		}

	} else { // if (selected_window) {

		// ========================================
		// Deactivation
		// ========================================

		bool mod1_up = key_code == config.hotkey_apps.mod && !key_down;
		bool mod2_up = key_code == config.hotkey_windows.mod && !key_down;

		if (mod1_up || mod2_up) {
			// Alt keyup - switch to selected window
			// Always pass/never consume alt/mod key up, because:
			// 1) cmdtab never hooks Alt keydowns
			// 2) cmdtab hooks Alt keyups, but always passes it through on pain
			//    of getting a stuck Alt key because of point 1
			show(*selected_window);
			cancel(false);
			goto pass_message; // See note above on "Alt keyup" on why I don't consume this message
		} else {

			bool key_repeat = setkey(key_code, key_down); // Manually track key repeat
			bool shift_held = getkey(VK_LSHIFT) || getkey(VK_RSHIFT) || getkey(VK_SHIFT);

			// ========================================
			// Navigation
			// ========================================

			bool key1_down = key_code == config.hotkey_apps.key && key_down;
			bool key2_down = key_code == config.hotkey_windows.key && key_down;
			bool prev_down = (key_code == VK_UP || key_code == VK_LEFT) && key_down;
			bool next_down = (key_code == VK_DOWN || key_code == VK_RIGHT) && key_down;

			// Subsequent Alt-Tab - next app (hold Shift for previous)
			// Also arrow keys, left and right
			if (key1_down || (next_down || prev_down)) {
				selectnext(true, shift_held || prev_down, !config.wrap_bump || !key_repeat);
				if (config.show_gui_for_apps) {
					resizegui(); // NOTE Must call resizegui after activate, before redrawgui & before showgui
					redrawgui();
					showgui(config.show_gui_delay);
				}
				if (config.fast_switching_apps) {
					peek(*selected_window);
				}
				// Since I "own" this hotkey system-wide, I consume this message unconditionally
				// This will consume the key1_down event. Windows will not know that key1 was pressed
				goto consume_message;
			}

			// Subsequent Alt-Tilde/Backquote - next window (hold Shift for previous)
			// TODO Also arrow keys, up and down
			if (key2_down) {
				//if (foregrounded_window) {
					selectnext(false, shift_held, !config.wrap_bump || !key_repeat);
				//}
				if (config.show_gui_for_windows) {
					resizegui(); // NOTE Must call resizegui after activate, before redrawgui & before showgui
					redrawgui();
					showgui(config.show_gui_delay);
				}
				if (config.fast_switching_windows) {
					peek(*selected_window);
				}
				// Since I "own" this hotkey system-wide, I consume this message unconditionally
				// This will consume the key2_down event. Windows will not know that key2 was pressed
				goto consume_message;
			}

			// ========================================
			// Deactivation
			// ========================================

			bool esc_down = key_code == VK_ESCAPE && key_down;
			bool enter_down = key_code == VK_RETURN && key_down;

			// Alt-Enter - switch to selected window
			if (enter_down) {
				show(*selected_window);
				cancel(false);
				// Since our swicher is active, I "own" this hotkey, so I consume this message
				// This will consume the enter_down event. Windows will not know that Enter was pressed
				goto consume_message;
			}
			// Alt-Esc - cancel switching and restore initial window
			if (esc_down) { // BUG There's a bug here if mod1_held and mod2_held aren't both the same key. In that case I'll still pass through the key event, even though I should own it since it's not Alt-Esc
				cancel(config.restore_on_cancel);
				// Since our swicher is active, I "own" this hotkey, so I consume this message
				// This will consume the esc_down event. Windows will not know that Escape was pressed
				goto consume_message;
			}

			// ========================================
			// Extended window management
			// ========================================

			bool keyF4_down  = key_code == VK_F4 && key_down;
			bool keyQ_down   = key_code == 'Q' && key_down;
			bool keyW_down   = key_code == 'W' && key_down;
			bool delete_down = key_code == VK_DELETE && key_down;
			bool keyM_down   = key_code == 'M' && key_down;
			bool keyH_down   = key_code == 'H' && key_down;
			bool keyB_down   = key_code == 'B' && key_down;

			// Alt-F4 - quit cmdtab
			if (keyF4_down) {
				close(gui);
				goto consume_message;
			}
			// Alt-Q - close all windows of selected application
			if (keyQ_down) {
				print(L"Q\n");
				goto consume_message;
			}
			// Alt-W or Alt-Delete - close selected window
			if (keyW_down || delete_down) {
				close(*selected_window);
				resetwindows();
				resizegui(); // NOTE Must call resizegui after activate, before redrawgui & before showgui
				redrawgui();
				goto consume_message;
			}
			// Alt-M - minimize selected window
			if (keyM_down) {
				minimize(*selected_window);
				goto consume_message;
			}
			// Alt-H - hide selected window
			if (keyH_down) {
				hide(*selected_window);
				goto consume_message;
			}
			// Alt-B - display app name and window class to user, to add in blacklist
			if (keyB_down) {
				snitch(*selected_window);
				cancel(false);
				goto consume_message;
			}

			// ========================================
			// Debug
			// ========================================

			bool keyF11_down = key_code == VK_F11 && key_down;
			bool keyF12_down = key_code == VK_F12 && key_down;
			bool keyPrtSc_down = key_code == VK_SNAPSHOT && key_down;

			if (keyF12_down) {
				debug();
			}
			if (keyF11_down) {
				printapps();
			}

			// ========================================
			// Hook management
			// ========================================

			// Pass through modifiers so they don't get stuck
			if (key_code == VK_CONTROL || key_code == VK_LCONTROL || key_code == VK_RCONTROL ||
				key_code == VK_MENU    || key_code == VK_LMENU    || key_code == VK_RMENU    ||
				key_code == VK_SHIFT   || key_code == VK_LSHIFT   || key_code == VK_RSHIFT   ||
				key_code == VK_CAPITAL) {
				print(L"modifier passthrough\n");
				goto pass_message;
			}
			// Input sink - consume keystrokes when activated
			if (!keyPrtSc_down) {
				print(L"input sink\n");
				goto consume_message;
			}
		}
	}

	pass_message:
		//print(L"next hook\n");
		return CallNextHookEx(null, code, wparam, lparam);
	consume_message:
		// By returning a non-zero value from the hook procedure, the message
		// is consumed and does not get passed to the target window
		//print(L"NOT next hook\n");
		return 1; 
}
static  i64 WndProc(handle hwnd, u32 message, u64 wparam, i64 lparam)
{
	switch (message) {
		case WM_CREATE: {
			resetwindows();
			break;
		}
		case WM_ERASEBKGND: {
			return 1;
		}
		case WM_PAINT: {
			// Double buffered drawing:
			// I draw off-screen in-memory to 'gdi_context' whenever changes occur,
			// and whenever I get the WM_PAINT message I just blit the
			// off-screen buffer to screen ("screen" means the 'hdc' returned by
			// BeginPaint() passed our 'hwnd')
			PAINTSTRUCT ps = {0};
			handle context = BeginPaint(hwnd, &ps);
			BitBlt(context, 0, 0, gdi_rect.right - gdi_rect.left, gdi_rect.bottom - gdi_rect.top, gdi_context, 0, 0, SRCCOPY);
			//int scale = 1.0;
			//SetStretchBltMode(gdi_context, HALFTONE);
			//StretchBlt(hdc, 0, 0, gdi_width, gdi_height, gdi_context, 0, 0, gdi_width * scale, gdi_height * scale, SRCCOPY);
			EndPaint(hwnd, &ps);
			ReleaseDC(hwnd, context);
			return 0;
		}
		case WM_ACTIVATE: { // WM_ACTIVATEAPP 
			// Cancel on mouse clicking outside window
			if (wparam == WA_INACTIVE) {
				// This rarely happens atm with the current implementation
				// because I don't set the switcher as foreground window when
				// showing! I just show the window and rely on being topmost.
				// But the user can click the switcher window and make it the
				// foreground window, in which case it will get this message
				// when the foreground window changes again.
				// I'm not sure if the implementation will remain like this
				// (i.e. not becoming foreground)--it's a little unorthodox. 
				// But for now that's how it is.
				print(L"gui lost focus\n");
				//return 1; // Nu-uh!
				//cancel(false); // NOTE See comments in cancel
			} else {
				print(L"gui got focus\n");
				//return 0;
				//synckeys();
			}
			break;
		}
		case WM_MOUSEMOVE: {
			mouse_x = LOWORD(lparam);
			mouse_y = HIWORD(lparam);
			//print(L"x%i y%i\n", mouse_x, mouse_y);
			TrackMouseEvent(&(TRACKMOUSEEVENT){.cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = gui});
			break;
		}
		case WM_MOUSELEAVE: {
			mouse_x = 0;
			mouse_y = 0;
			//print(L"x%i y%i\n", mouse_x, mouse_y);
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
static void WinEventProc(handle hook, u32 event, handle hwnd, i32 objectid, i32 childid, u32 threadid, u32 timestamp) 
{
	if (event == EVENT_SYSTEM_FOREGROUND) {
		if (selected_window) { // If switcher is active, window activations are queued to preserve app and window order
			if (queue_count >= countof(queue)) {
				flushqueue(); // Flush queue if queue is about to overflow.
				redrawgui(); // Yes, this causes weird looking GUI update. Yes, that's a very good tradeoff
			}
			queue[queue_count++] = hwnd;
			print(L"queued %p\n", hwnd);
		} else {
			addwindow(hwnd);
		}
	}
}

int WINAPI wWinMain(handle hInstance, handle hPrevInstance, u16 *args, int show) {

//#ifdef _DEBUG
//	FILE *conout = null;
//	AllocConsole();
//	freopen_s(&conout, "CONOUT$", "w", stdout);
//#endif

	// If "--autorun" is not passed as arg, prompt about autorun
	if (wcscmp(args, L"--autorun")) { // lstrcmpW, CompareStringW
		autorun(ask(L"Start cmdtab automatically?\nRelaunch cmdtab.exe to change your mind."), L"cmdtab", L"--autorun");
	}
	
	// Quit if another instance of cmdtab is already running
	handle hMutex = CreateMutexW(null, true, L"cmdtab_mutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		MessageBoxW(null, L"cmdtab is already running.", L"cmdtab", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
		return 0;
	}
	
	// Initialize runtime-dependent settings. This one is dependent on user's current kbd layout
	config.hotkey_windows.key = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX);
	
	// Install (low level) keyboard hook
	handle hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, (HOOKPROC)KeyboardProc, null, 0);
	// Install window event hook for one event: foreground window changes. Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20130930-00/?p=3083
	handle hWinEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, null, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
	
	// Create window
	WNDCLASSEXW wcex = {0};
	wcex.cbSize = sizeof wcex;
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)WndProc;
	wcex.hInstance = hInstance;
	wcex.lpszClassName = L"cmdtab";
	gui = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, RegisterClassExW(&wcex), null, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, null, null, hInstance, null);
	
	// Clear all window styles for very plain window
	SetWindowLongW(gui, GWL_STYLE, 0);
	
	// SetThemeAppProperties(null); // lol x)
	
	print(L"darkmode %s\n", getdarkmode() ? L"YES" : L"NO");

	// Set the *title bar* to dark mode
	// For dark mode details, see: https://stackoverflow.com/a/70693198/659310
	BOOL darktitlebar = TRUE;
	DwmSetWindowAttribute(gui, DWMWA_USE_IMMERSIVE_DARK_MODE, &darktitlebar, sizeof darktitlebar);

	// Rounded window corners
	DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
	DwmSetWindowAttribute(gui, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners);

	// Start msg loop for thread
	MSG msg;
	while (GetMessageW(&msg, null, 0, 0) > 0) DispatchMessageW(&msg);

	// Done
	UnhookWindowsHookEx(hKeyboardHook);
	//UnhookWinEvent(hWinEventHook);
	//UnhookWinEvent(hWinEventHook2);
	//CloseHandle(hKeyboardHook);
	if (hMutex) {
		ReleaseMutex(hMutex);
		//CloseHandle(hMutex);
	}
	//DestroyWindow(gui);

	print(L"done\n");
	_CrtDumpMemoryLeaks();
	return (int)msg.wParam;
}
