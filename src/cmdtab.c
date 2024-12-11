//#include <stdlib.h>
#include <math.h>

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define _CRTDBG_MAP_ALLOC

#define NOGDICAPMASKS
//#define NOVIRTUALKEYCODES
//#define NOWINMESSAGES
//#define NOWINSTYLES
//#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
//#define NOSYSCOMMANDS
//#define NORASTEROPS
//#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
//#define NOCTLMGR
//#define NODRAWTEXT
//#define NOGDI
#define NOKERNEL
//#define NOUSER
#define NONLS
//#define NOMB
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
//#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
//#define NOWH
//#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

#include <windows.h>
#include <crtdbg.h>
//#include "winuser.h"                // Used by GetWindowTitle, GetFilepath, filter, link, next, user32.lib
#include "shellapi.h"               // Used by GetAppIcon, shell32.lib
#include "shlwapi.h"                // Used by GetAppName, shell32.lib?
#include "dwmapi.h"                 // Used by filter, dwmapi.lib
//#include "processthreadsapi.h"      // OpenProcess, kernel32.lib
//#include "winbase.h"                // QueryFullProcessImageNameW, kernel32.lib
//#include "handleapi.h"              // CloseHandle, kernel32.lib
#include "pathcch.h"                // Used by GetFilename
#include "strsafe.h"                // Used by GetAppName
#include "commoncontrols.h"         // Used by GetAppIcon, needs COBJMACROS, comctl.lib

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "pathcch.lib")
#pragma comment(lib, "version.lib") // Used by GetAppName

//==============================================================================
// Linker directives
//==============================================================================

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")

//==============================================================================
// Utility belt
//==============================================================================

//#define min(a,b) (((a)<(b))?(a):(b))
//#define max(a,b) (((a)>(b))?(a):(b))
#define countof(a) (ptrdiff_t)(sizeof(a)/sizeof(*a))
#define string(str) {str,countof(str)-1,true}

#define false 0
#define true 1
#define null NULL // WHY IS EVERYONE SCREAMING

typedef wchar_t             u16; // "Microsoft implements wchar_t as a two-byte unsigned value."
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed int          i32;
typedef signed long long    i64;
typedef signed int         bool;
typedef ptrdiff_t          size;
typedef void *           handle;
typedef struct string    string;

struct string {
	u16 text[MAX_PATH];
	size length;
	bool ok;
};

static const string UWPAppHostExe      = string(L"ApplicationFrameHost.exe");
static const string UWPAppHostClass    = string(L"ApplicationFrameWindow");
static const string UWPCoreWindowClass = string(L"Windows.UI.Core.CoreWindow");
static const string UWPAppsPath        = string(L"C:\\Program Files\\WindowsApps");

static void CopyString(string *src, string *dst)
{

}
static bool StringsAreEqual(string *s1, string *s2)
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
static bool StringEndsWith(string *s1, string *s2)
{
	u16 *str = s1->text;
	u16 *fix = s2->text;
	size str_len = s1->length;
	size fix_len = s2->length;
	return str_len >= fix_len && !memcmp(str+str_len-fix_len, fix, fix_len);
}

static void Debug(u16 *fmt, ...)
{
	#ifdef _DEBUG
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	OutputDebugStringW(buffer);
	__debugbreak(); // Or DebugBreak in debugapi.h
	#endif
}
static void Print(u16 *fmt, ...)
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
static void Error(handle hwnd, u16 *fmt, ...)
{
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	OutputDebugStringW(buffer);
	MessageBoxW(hwnd, buffer, L"cmdtab", MB_OK | MB_ICONERROR | MB_TASKMODAL);
}
static bool Ask(handle hwnd, u16 *fmt, ...)
{
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	return MessageBoxW(hwnd, buffer, L"cmdtab", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL) == IDYES;
}

static i64 StartMeasuring(void)
{
	i64 start;
	QueryPerformanceCounter((LARGE_INTEGER *)&start);
	return start;
}
static i64 FinishMeasuring(i64 start)
{
	i64 end, frequency;
	QueryPerformanceCounter((LARGE_INTEGER *)&end);
	QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);
	return ((end - start) * 1000) / frequency;
}

static string GetWindowClass(handle hwnd)
{
	string class = {0};
	class.length = GetClassNameW(hwnd, class.text, countof(class.text));
	class.ok = (class.length > 0);
	return class;
}
static bool _GetCoreWindow(handle child, i64 lparam)
{
	// EnumChildProc used by GetCoreWindow
	// Set inout 'lparam' to hosted core window
	string class = GetWindowClass(child);
	if (StringsAreEqual(&class, &UWPCoreWindowClass)) {
		*(handle *)lparam = child;
		return false; // Found the hosted core window, so we're done
	} else {
		return true; // Return true to continue EnumChildWindows
	}
}
static handle GetCoreWindow(handle hwnd)
{
	string class = GetWindowClass(hwnd);
	if (StringsAreEqual(&class, &UWPAppHostClass)) {
		handle hwnd2 = hwnd;
		EnumChildWindows(hwnd, _GetCoreWindow, &hwnd2); // _GetCoreWindow sets passed param to hosted window, if any
		return hwnd2;
	} else {
		return hwnd;
	}
}
static handle GetAppHost(handle hwnd)
{
	string class = GetWindowClass(hwnd);
	if (StringsAreEqual(&class, &UWPCoreWindowClass)) {
		return GetAncestor(hwnd, GA_PARENT); // TODO Test
	} else {
		return hwnd;
	}
}

static bool IsAltTabWindow(handle hwnd)
{
	// Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
	if (!IsWindowVisible(hwnd)) {
		return false;
	}
	int cloaked = 0;
	DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked); // Windows 8
	if (cloaked) {
		return false;
	}
	// "A window with the WS_EX_APPWINDOW extended style is treated as if it has no owner,
	// even if it does."
	if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) {
		return true;
	}
	// "The window should not be activated through programmatic access or via keyboard
	// navigation by accessible technology, such as Narrator."
	// "The window does not appear on the taskbar by default."
	if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
		return false;
	}
	// "A tool window [i.e. WS_EX_TOOLWINDOW] does not appear in the taskbar or
	// in the dialog that appears when the user presses ALT-TAB." (docs)
	if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
		return false;
	}
	return true;
}

static string GetFilepath(handle hwnd)
{
	// How to get the filepath of the executable for the process that spawned the window:
	// 1. GetWindowThreadProcessId to get process id for window
	// 2. OpenProcess for access to remote process memory
	// 3. QueryFullProcessImageNameW [Windows Vista] to get full "executable image" name (i.e. exe path) for process

	string path = {0};
	u32 pid;
	GetWindowThreadProcessId(hwnd, &pid);
	handle process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid); // NOTE See GetProcessHandleFromHwnd
	if (!process || process == INVALID_HANDLE_VALUE) {
		Print(L"WARNING couldn't open process with pid: %u\n", pid);
		return path;
	}
	u32 length = countof(path.text);
	path.ok = QueryFullProcessImageNameW(process, 0, path.text, &length); // NOTE Can use GetLongPathName if getting shortened paths
	CloseHandle(process);
	if (!path.ok) {
		Print(L"WARNING couldn't get exe path for process with pid: %u\n", pid);
		return path;
	}
	path.length = length;
	return path;
}
static string GetFilename(u16 *filepath, bool extension)
{
	// Just yer normal filepath string manipulation. Aka. basename?
	string name = {0};
	name.ok = SUCCEEDED(StringCchCopyNW(name.text, countof(name.text), PathFindFileNameW(filepath), countof(name.text)));
	if (!extension) {
		name.ok = name.ok && SUCCEEDED(PathCchRemoveExtension(name.text, countof(name.text)));
	}
	name.ok = name.ok && SUCCEEDED(StringCchLengthW(name.text, countof(name.text), &name.length));
	return name;
}
static string GetWindowTitle(handle hwnd)
{
	string title = {0};
	title.length = GetWindowTextW(hwnd, title.text, countof(title.text));
	title.ok = (title.length > 0);
	return title;
}
static string GetAppName(u16 *filepath)
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
	if (!GetFileVersionInfoW(filepath, handle = 0, size, version)) {
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
static handle GetAppIcon(u16 *filepath)
{
	// From Raymond Chen: https://stackoverflow.com/a/12429114/659310
	//
	// Requires the following:
	//
	// #define COBJMACROS
	// #include "commoncontrols.h"
	//
	// SHGetFileInfoW docs:
	//   "You should call this function from a background thread. Failure to do
	//    so could cause the UI to stop responding." (docs)
	//   "If SHGetFileInfo returns an icon handle in the hIcon member of the
	//    SHFILEINFO structure pointed to by psfi, you are responsible for
	//    freeing it with DestroyIcon when you no longer need it." (docs)
	//   "You must initialize Component Object Model (COM) with CoInitialize or
	//    OleInitialize prior to calling SHGetFileInfo." (docs)
	//
	// NOTE: Can also check out SHDefExtractIcon
	//       https://devblogs.microsoft.com/oldnewthing/20140501-00/?p=1103

	if (FAILED(CoInitialize(null))) { // In case we use COM
		return null;
	}
	SHFILEINFOW info = {0};
	if (!SHGetFileInfoW(filepath, 0, &info, sizeof info, SHGFI_SYSICONINDEX)) { // 2nd arg: -1, 0 or FILE_ATTRIBUTE_NORMAL
		Debug(L"suspicious SHGetFileInfoW return"); // SHGetFileInfoW has special return with SHGFI_SYSICONINDEX flag, which I think should never fail (docs unclear?)
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

static bool IsDarkModeEnabled(void)
{
	u32 value;
	u32 size = sizeof value;
	if (!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUsesLightTheme", RRF_RT_DWORD, null, &value, &size)) {
		return !value; // Value is true for light mode
	} else {
		return true; // Default to dark mode
	}
}
static u32 GetAccentColor(void)
{
	u32 value;
	u32 size = sizeof value;
	if (!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM\\", L"AccentColor", RRF_RT_DWORD, null, &value, &size)) {
		return value;
	} else {
		//       AABBGGRR
		return 0xFFFFC24C; // Default to aqua color, sampled from Windows 11 Alt-Tab
	}
}

static void SetAutorun(bool enabled, u16 *keyname, u16 *args)
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

static bool IsKeyDown(u32 key)
{
	return (bool)(GetAsyncKeyState(key) & 0x8000);
}

static void PrintWindowHandle(handle hwnd) // PrintWindow already taken
{
	if (hwnd) {
		string filepath = GetFilepath(hwnd);
		if (!filepath.ok) return;
		string filename = GetFilename(filepath.text, false);
		string class = GetWindowClass(hwnd);
		string title = GetWindowTitle(hwnd);
		u16 *wsIconic           = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_ICONIC) ? L" (minimized) " : L" ";
		u16 *wsVisible          = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_VISIBLE) ? L"visible" : L"not visible";
		u16 *wsChild            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) ? L", child" : L", parent";
		u16 *wsDisabled         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DISABLED) ? L", disabled" : L", enabled";
		u16 *wsPopup            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUP) ? L", popup" : L"";
		u16 *wsPopupWindow      = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUPWINDOW) ? L", popupwindow" : L"";
		u16 *wsTiled            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_TILED) ? L", tiled" : L"";
		u16 *wsTiledWindow      = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_TILEDWINDOW) ? L", tiledwindow" : L"";
		u16 *wsOverlapped       = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPED) ? L", overlapped" : L"";
		u16 *wsOverlappedWindow = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW) ? L", overlappedwindow" : L"";
		u16 *wsDlgFrame         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DLGFRAME) ? L", dlgframe" : L"";
		u16 *exToolWindow       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ? L", toolwindow" : L"";
		u16 *exAppWindow        = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) ? L", appwindow" : L"";
		u16 *exDlgModalFrame    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_DLGMODALFRAME) ? L", dlgmodalframe" : L"";
		u16 *exLayered          = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) ? L", layered" : L"";
		u16 *exNoActivate       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) ? L", noactivate" : L"";
		u16 *exPaletteWindow    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_PALETTEWINDOW) ? L", palettewindow" : L"";
		u16 *exOverlappedWindow = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_OVERLAPPEDWINDOW) ? L", exoverlappedwindow" : L"";

		Print(L"%p { %s, %s }%s\"%.32s\"", hwnd, filename.text, class.text, wsIconic, title.text);
		Print(L" (%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s)\n",
			wsVisible, wsChild, wsDisabled, wsPopup, wsPopupWindow, wsTiled, wsTiledWindow, wsOverlapped, wsOverlappedWindow, wsDlgFrame,
			exToolWindow, exAppWindow, exDlgModalFrame, exLayered, exNoActivate, exPaletteWindow, exOverlappedWindow);
	} else {
		Print(L"null hwnd\n");
	}
}
static void ReportWindowHandle(handle hwnd, handle target)
{
	// Tell user the executable name and class so they can add it to the blacklist
	string class = GetWindowClass(target);
	string path = GetFilepath(target);
	string name = GetFilename(path.text, false);

	u16 buffer[2048];
	StringCchPrintfW(buffer, countof(buffer)-1, L"{%s, %s}\n\nAdd to blacklist?\n\nNOT WORKNG YET, DOES NOTHING, TODO!", name.text, class.text);
	return MessageBoxW(hwnd, buffer, L"cmdtab", MB_YESNO | MB_ICONASTERISK | MB_TASKMODAL) == IDYES;
}

static void StealFocus(void)
{
	// Oh my, the fucking rabbit hole...
	// This is a hack (one of many flavors) to bypass the restrictions on setting the foreground window
	// https://github.com/microsoft/PowerToys/blob/7d0304fd06939d9f552e75be9c830db22f8ff9e2/src/modules/fancyzones/FancyZonesLib/util.cpp#L376
	//
	// There is also a subtle issue with the Alt-key functionality of windows with menus, where
	// the menu selection will get stuck until the window gets an AltUp event. It doesn't seem
	// to help to send the AltUp event globally; I think the window must receive it to reset its
	// menu selection. But this differs from Windows' normal AltTab widget, which somehow bypasses
	// this issue. I've confirmed that the AltTab window does something different because there are
	// subtle observable behavior differences in the menu selection when Windows' normal AltTab is
	// invoked vs. cmdtab. In the end, I figured out that instead of sending an AltUp event, if I
	// just steal focus as quickly as possible upon activation, the menu selection does not get stuck,
	// and the behavior exhibited perfectly matches Windows' normal AltTab. Success!
	SendInput(1, &(INPUT){.type = INPUT_KEYBOARD}, sizeof(INPUT));
}

static void DisplayWindow(handle hwnd) // ShowWindow already taken
{
	if (GetForegroundWindow() != hwnd) {
		if (IsIconic(hwnd)) {
			ShowWindow(hwnd, SW_RESTORE);
		} else {
			ShowWindow(hwnd, SW_SHOWNA);
		}
		if (SetForegroundWindow(GetAppHost(hwnd))) {
			Print(L"switch to ");
			PrintWindowHandle(hwnd);
		} else {
			Error(null/*Switcher*/, L"ERROR couldn't switch to %s", GetWindowTitle(hwnd));
			PrintWindowHandle(hwnd);
			//CancelSwitcher(false);
		}
	} else {
		Print(L"switch to: already foreground\n");
	}
}
static void PreviewWindow(handle hwnd)
{
	//typedef HRESULT (__stdcall *DwmpActivateLivePreview)(bool peekOn, handle hPeekWindow, handle hTopmostWindow, u32 peekType1or3, i64 newForWin10);
	//DwmpActivateLivePreview(true, hwnd, gui, 3, 0);

	// TODO "perviewing" not implemented yet. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)

	// NOT IMPLEMENTED YET, FALLBACK TO SHOW
	DisplayWindow(hwnd);
}
static void HideWindow(handle hwnd)
{
	// WARNING Can't use this because we filter out hidden windows
	ShowWindow(hwnd, SW_HIDE); // Yes, call ShowWindow to hide window...
}
static void MinimizeWindow(handle hwnd)
{
	PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}
static void QuitWindow(handle hwnd) // CloseWindow already taken
{
	Print(L"close window ");
	PrintWindowHandle(hwnd);
	PostMessageW(hwnd, WM_CLOSE, 0, 0);
}
static void TerminateWindowProcess(handle hwnd)
{
	u32 pid;
	GetWindowThreadProcessId(hwnd, &pid);
	handle process = OpenProcess(PROCESS_TERMINATE, false, pid); // NOTE See GetProcessHandleFromHwnd
	if (!process || process == INVALID_HANDLE_VALUE || !TerminateProcess(process, 0)) {
		Print(L"ERROR couldn't terminate process with pid: %u\n", pid);
	}
	if (process) CloseHandle(process);
}

//==============================================================================
// cmdtab impl
//==============================================================================

// Go from supporting max. 256 windows @ 690kb to supporting max. 8192 windows @ 340kb
//  (49% memory decrease, 3200% capacity increase),
//  but sectioned into max. 128 apps, each with max. 64 windows
//  And then again reduce another 40%, from 340kb to 200kb, by reducing string size from MAX_PATH*2 to MAX_PATH
#define APPS_MAX     128
#define WINDOWS_MAX   64
#define BLACKLIST_MAX 32

struct config {
	// Hotkeys
	struct { u32 mod, key; } hotkeyForApps;
	struct { u32 mod, key; } hotkeyForWindows;
	// Behavior
	bool switchApps;
	bool switchWindows;
	bool fastSwitchingForApps; // TODO
	bool fastSwitchingForWindows;
	bool showSwitcherForApps;
	bool showSwitcherForWindows;
	u32 showSwitcherDelay;
	bool wrapbump;
	bool ignoreMinimized; // TODO
	bool restoreOnCancel;
	// Appearance
	bool darkmode;
	u32 switcherHeight;
	u32 switcherHorzMargin;
	u32 switcherVertMargin;
	u32 iconWidth;
	u32 iconHorzPadding;
	// Blacklist
	struct identifier {
		u16 *filename;            // Filename without file extension, ex. "explorer". Can be null.
		u16 *windowClass;         // Window class name. Can be null.
	} blacklist[BLACKLIST_MAX];
};

struct app {
	string path;                  // Full path of app executable.
	string name;                  // Product description/name, or filename without extension as fallback.
	handle icon;                  // Big app icon.
	handle windows[WINDOWS_MAX];  // App windows that can be switched to.
	size windowsCount;            // Number of elements in 'windows' array.
};

static struct app Apps[APPS_MAX]; // Apps that are displayed in switcher.
static size AppsCount;            // Number of elements in 'Apps' array.
static struct app *SelectedApp;   // Pointer to one of the elements in 'Apps' array. The app for the 'SelectedWindow'.
static handle *SelectedWindow;    // Currently selected window in switcher. Non-NULL indicates switcher is active.
static handle RestorableWindow;   // Whatever window was foreground when switcher was activated. Does not need to be in filtered 'Apps' array.
static handle History[APPS_MAX];  // TODO REWRITE Defer window activations while the switcher is active to preserve app and window order in switcher.
static size HistoryCount;         // TODO REWRITE The activation queue is committed when the switcher deactivates or if the queue is about to overflow.
static handle Switcher;           // Handle for main window (i.e. switcher).
static handle DrawingContext;     // Drawing context for double-buffered drawing of main window.
static handle DrawingBitmap;      // Off-screen bitmap used for double-buffered drawing of main window.
static RECT DrawingRect;          // Size of the off-screen bitmap.
static u16 Keyboard[16];          // 256 bits to track key repeat for low-level keyboard hook.
static i32 MouseX, MouseY;        // Mouse position, for highlighting and clicking app icons.
static struct config Config;      // cmdtab settings

static void PrintApps(void)
{
	int windowsCount = 0;
	for (int i = 0; i < AppsCount; i++) {
		Print(L"%i ", i);
		struct app *app = &Apps[i];
		PrintWindowHandle(app->windows[0]);
		windowsCount += app->windowsCount;
		for (int j = 1; j < app->windowsCount; j++) {
			Print(L"  %i ", j);
			PrintWindowHandle(app->windows[j]);
		}
	}
	Print(L"%i apps %i windows, %i history:\n", AppsCount, windowsCount, HistoryCount);
	// And print History
	for (int i = 0; i < HistoryCount; i++) {
		Print(L"%i ", i);
		PrintWindowHandle(History[i]);
	}
}

static bool WindowIsBlacklisted(string *filename, string *windowClass)
{
	#define wcseq(str1, str2) (wcscmp(str1, str2) == 0)

	// Iterate blacklist. I allow three kinds of entries in the black list:
	// 1) {name,class} both name and class are specified and both must match
	// 2) {name,null}  only name is specified and must match
	// 3) {null,class} only class is specified and must match
	// 4) {null,null}  terminates array
	for (size i = 0; i < countof(Config.blacklist); i++) {
		struct identifier ignored = Config.blacklist[i];
		if (ignored.filename && ignored.windowClass) {
			if (wcseq(ignored.filename, filename->text) &&
				wcseq(ignored.windowClass, windowClass->text)) {
				return true;
			}
		} else if (ignored.filename && !ignored.windowClass && wcseq(ignored.filename, filename->text)) {
			return true;
		} else if (!ignored.filename && ignored.windowClass && wcseq(ignored.windowClass, windowClass->text)) {
			return true;
		} else if (!ignored.filename && !ignored.windowClass) {
			break;
		}
	}
	return false;
}

static void AddToHistory(handle hwnd) {
	// Get hosted window in case of UWP host
	hwnd = GetCoreWindow(hwnd);

	Print(L"activate window ");
	PrintWindowHandle(hwnd);

	if (History[0] == hwnd) {
		Print(L"  already first in history\n");
		return;
	}
	// Potentially find 'hwnd' in activation list and shift other hwnds down to position of 'hwnd' and then put 'hwnd' first
	for (int i = 1; i < HistoryCount; i++) {
		if (History[i] == hwnd) {
			memmove(&History[1], &History[0], sizeof History[0] * i);
			History[0] = hwnd;
			return;
		}
	}

	// 'hwnd' doesn't already exist in activation list; so validate and add it

	// Basic checks for eligibility in Alt-Tab
	if (!IsAltTabWindow(hwnd)) {
		Print(L"ignored ");
		PrintWindowHandle(hwnd);
		return;
	}
	if (Config.ignoreMinimized && IsIconic(hwnd)) { // TODO Hmm, rework or remove this feature. Atm, 'ignoreMinimized' affects both Alt-Tilde/Backquote and Alt-Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.) TODO Should this be called 'restore_minimized'?
		return;
	}
	// Get module filepath, abort if failed (most likely access denied)
	string filepath = GetFilepath(hwnd);
	if (!filepath.ok) {
		return; // Don't have to print error, since GetFilepath already does that
	}
	// Check window against user-defined blacklist
	string filename = GetFilename(filepath.text, false);
	string class = GetWindowClass(hwnd);
	if (WindowIsBlacklisted(&filename, &class)) {
		Print(L"blacklisted ");
		PrintWindowHandle(hwnd);
		return;
	}

	// 'hwnd' is valid and should be added

	// Shift all hwnds down one position and then put 'hwnd' first
	if (HistoryCount < countof(History)) {
		memmove(&History[1], &History[0], sizeof History[0] * ++HistoryCount);
		History[0] = hwnd;
	}
}
static void AddToSwitcher(handle hwnd)
{
	// 1. Get hosted window in case of UWP host
	/*dbg*/bool isUWP = false;
	handle coreWindow = GetCoreWindow(hwnd);
	if (hwnd != coreWindow) {
		hwnd = coreWindow;
		/*dbg*/isUWP = true;
	}
	// 2. Basic checks for eligibility in Alt-Tab
	if (!IsAltTabWindow(hwnd)) {
		Print(L"ignored ");
		PrintWindowHandle(hwnd);
		return;
	}
	if (Config.ignoreMinimized && IsIconic(hwnd)) {
		// TODO Hmm, rework or remove this feature
		// Atm, 'ignoreMinimized' affects both Alt-Tilde/Backquote and Alt-Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.) TODO Should this be called 'restore_minimized'?
		return;
	}
	// 3. Get module filepath, abort if failed (most likely access denied)
	string filepath = GetFilepath(hwnd);
	if (!filepath.ok) {
		return; // Don't have to print error, since GetFilepath already does that
	}
	// 4. Check window against user-defined blacklist
	string filename = GetFilename(filepath.text, false);
	string class = GetWindowClass(hwnd);
	if (WindowIsBlacklisted(&filename, &class)) {
		Print(L"blacklisted ");
		PrintWindowHandle(hwnd);
		return;
	}
	// 5. Find existing app with same module filepath
	struct app *app = null;
	for (int i = 0; i < AppsCount; i++) {
		if (StringsAreEqual(&filepath, &Apps[i].path)) {
			app = &Apps[i];
			break;
		}
	}
	// 6. If app not already tracked, create new app entry
	if (!app) {
		if (AppsCount >= countof(Apps)) {
			Error(Switcher, L"ERROR reached max. apps\n");
			return;
		}
		string name = GetAppName(filepath.text);
		if (!name.ok) {
			name = filename; // filename from earlier when checking blacklist
		}
		// Since I reset by truncating (setting AppsCount to 0), make sure to overwrite all struct fields when reusing array slots
		app = &Apps[AppsCount++];
		if (app->icon) DestroyIcon(app->icon); // In fact, I have to do a little lazy cleanup
		app->path = filepath;
		app->name = name;
		app->icon = GetAppIcon(filepath.text);
		app->windowsCount = 0; // Same truncation trick here (but hwnds are not structs, so no fields to overwrite)
	}
	if (app->windowsCount >= countof(app->windows)) {
		Error(Switcher, L"ERROR reached max. windows for app\n");
		return;
	}
	// 7. Add window to app's window list
	app->windows[app->windowsCount++] = hwnd;
	Print(L"add window %s", isUWP ? L"(uwp) " : L"");
	PrintWindowHandle(hwnd);
}
static void ActivateWindow(handle hwnd)
{
	struct app *app = null;
	handle *window = null;
	for (int i = 0; i < AppsCount; i++) {
		app = &Apps[i];
		for (int j = 0; j < app->windowsCount; j++) {
			if (app->windows[j] == hwnd) {
				window = &app->windows[j];
				break;
			}
		}
		if (window) break;
	}
	// Put window first in app's window list and app first in app list
	if (window) {
		if (window > &app->windows[0]) { // Don't shift if already first
			// Shift other windows down to position of 'window' and then put 'window' first
			memmove(&app->windows[1], &app->windows[0], sizeof app->windows[0] * (window - app->windows)); // i.e. index
			app->windows[0] = hwnd;
		}
		if (app > &Apps[0]) { // Don't shift if already first
			// Shift other apps down to position of 'app' and then put 'app' first
			struct app temp = *app; // This is a 1.6kb copy
			memmove(&Apps[1], &Apps[0], sizeof Apps[0] * (app - Apps)); // i.e. index
			Apps[0] = temp;
		}
	}
	//Print(L"  already first in window list\n");
	//print(L"  window list before reorder\n");
	//for (int i = 0; i < app->windowsCount; i++) {
	//	print(L"    %i", i);
	//	printhwnd(app->windows[i]);
	//}
	//print(L"  window list after reorder\n");
	//for (int i = 0; i < app->windowsCount; i++) {
	//	print(L"    %i", i);
	//	printhwnd(app->windows[i]);
	//}
	//Print(L"  already first in app list\n");
	//print(L"  app list before reorder\n");
	//for (int i = 0; i < AppsCount; i++) {
	//	print(L"    %i", i);
	//	printhwnd(apps[i].windows[0]);
	//}
	//print(L"  app list after reorder\n");
	//for (int i = 0; i < AppsCount; i++) {
	//	print(L"    %i", i);
	//	printhwnd(apps[i].windows[0]);
	//}
}
static bool _AddToSwitcher(handle hwnd, i64 lparam)
{
	// EnumWindowsProc used by AddToSwitcher
	// Call AddToSwitcher with every top-level window enumerated by EnumWindows
	(*((int *)lparam))++;
	AddToSwitcher(hwnd);
	return true; // Return true to continue EnumWindows
}
static void UpdateApps(void)
{
	// Rebuild the whole window list and measure how long it takes
	i64 start = StartMeasuring();
	int windowsCount = 0;
	AppsCount = 0;
	EnumWindows(_AddToSwitcher, &windowsCount); // _AddToSwitcher calls AddToSwitcher with every top-level window enumerated by EnumWindows
	AddToHistory(GetForegroundWindow());
	for (int i = HistoryCount-1; i >= 0; i--) {
		ActivateWindow(History[i]);
	}
	Print(L"%i windows, %llims elapsed\n", windowsCount, FinishMeasuring(start));
	PrintApps();
}

static void SelectFirstWindow(void)
{
	if (AppsCount > 0) {
		SelectedApp = &Apps[0];
		SelectedWindow = &Apps[0].windows[0];
		RestorableWindow = GetForegroundWindow();
	} else {
		Print(L"nothing to select\n");
	}
}
static void SelectNextWindow(bool applevel, bool reverse, bool wrap)
{
	if (!SelectedApp || !SelectedWindow) {
		Debug(L"invalid state: no SelectedApp or no SelectedWindow");
	}
	/*dbg*/struct app *oldApp = SelectedApp;
	/*dbg*/handle *oldWindow = SelectedWindow;
	if (applevel) {
		struct app *firstApp = &Apps[0];
		struct app  *lastApp = &Apps[AppsCount-1];
		if (reverse) {
			if (SelectedApp > firstApp) {
				SelectedApp--;
			} else if (wrap) {
				SelectedApp = lastApp;
			}
		} else {
			if (SelectedApp < lastApp) {
				SelectedApp++;
			} else if (wrap) {
				SelectedApp = firstApp;
			}
		}
		SelectedWindow = &SelectedApp->windows[0];
	} else {
		handle *firstWindow = &SelectedApp->windows[0];
		handle  *lastWindow = &SelectedApp->windows[SelectedApp->windowsCount-1];
		if (reverse) {
			if (SelectedWindow > firstWindow) {
				SelectedWindow--;
			} else if (wrap) {
				SelectedWindow = lastWindow;
			}
		} else {
			if (SelectedWindow < lastWindow) {
				SelectedWindow++;
			} else if (wrap) {
				SelectedWindow = firstWindow;
			}
		}
	}
	/*dbg*/
	if (oldApp != SelectedApp || oldWindow != SelectedWindow) {
		Print(L"select ");
		PrintWindowHandle(*SelectedWindow);
	}
}

static void ResizeSwitcher(void)
{
	u32   iconsWidth = AppsCount * Config.iconWidth;
	u32 paddingWidth = AppsCount * Config.iconHorzPadding * 2;
	u32  marginWidth =         2 * Config.switcherHorzMargin;

	u32 w = iconsWidth + paddingWidth + marginWidth;
	u32 h = Config.switcherHeight;
	u32 x = GetSystemMetrics(SM_CXSCREEN) / 2 - (w / 2);
	u32 y = GetSystemMetrics(SM_CYSCREEN) / 2 - (h / 2);

	// Resize off-screen double-buffering bitmap
	RECT resized = {x, y, x+w, y+h};
	if (!EqualRect(&DrawingRect, &resized)) {
		handle context = GetDC(Switcher);
		DeleteObject(DrawingBitmap);
		DeleteDC(DrawingContext);
		DrawingRect = resized;
		DrawingContext = CreateCompatibleDC(context);
		DrawingBitmap = CreateCompatibleBitmap(context, DrawingRect.right - DrawingRect.left, DrawingRect.bottom - DrawingRect.top);
		handle oldBitmap = (handle)SelectObject(DrawingContext, DrawingBitmap);
		DeleteObject(oldBitmap);
		ReleaseDC(Switcher, context);
	}

	MoveWindow(Switcher, x, y, w, h, false); // Yes, "MoveWindow" means "ResizeWindow"
}
static void RedrawSwitcher(void)
{
	// TODO Use 'Config.style'

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

	static HBRUSH windowBackground = {0};
	static HBRUSH selectionBackground = {0};
	static HPEN selectionOutline = {0};

	// Init background brushes (static vars)
	if (windowBackground == null) {
		windowBackground = CreateSolidBrush(BACKGROUND);
	}
	if (selectionBackground == null) {
		selectionBackground = CreateSolidBrush(HIGHLIGHT_BG);
	}
	//if (selection_outline == null) {
	selectionOutline = CreatePen(PS_SOLID, SEL_OUTLINE, (GetAccentColor() & 0x00FFFFFF)); //HIGHLIGHT); // TODO Leak?
	//}

	RECT rect = {0};
	GetClientRect(Switcher, &rect);

	// Invalidate & draw window background
	RedrawWindow(Switcher, null, null, RDW_INVALIDATE | RDW_ERASE);
	FillRect(DrawingContext, &rect, windowBackground);

	// Select pen & brush for RoundRect (used to draw selection rectangle)
	HPEN oldPen = (HPEN)SelectObject(DrawingContext, selectionOutline);
	HBRUSH oldBrush = (HBRUSH)SelectObject(DrawingContext, selectionBackground);

	// Select text font, color & background for DrawTextW (used to draw app name)
	HFONT oldFont = (HFONT)SelectObject(DrawingContext, GetStockObject(DEFAULT_GUI_FONT));
	SetTextColor(DrawingContext, TEXT_COLOR);
	SetBkMode(DrawingContext, TRANSPARENT);

	// This is how GDI handles memory?
	//DeleteObject(oldPen);
	//DeleteObject(oldBrush);
	//DeleteObject(oldFont);

	for (int i = 0; i < AppsCount; i++) {
		struct app *app = &Apps[i];

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
		//bool app_is_mouseover = PtInRect(&icon_rect, (POINT){MouseX, MouseY});
		//print(L"mouse %s\n", app_is_mouseover ? L"over" : L"not over");

		if (app != SelectedApp) {
			// Draw only icon, with window background
			DrawIconEx(DrawingContext, left, top, app->icon, width, height, 0, windowBackground, DI_NORMAL);
		} else {
			// Draw selection rectangle and icon, with selection background
			RoundRect(DrawingContext, left - SEL_VERT_OFF, top - SEL_HORZ_OFF, right + SEL_VERT_OFF, bottom + SEL_HORZ_OFF, SEL_RADIUS, SEL_RADIUS);
			DrawIconEx(DrawingContext, left, top, app->icon, width, height, 0, selectionBackground, DI_NORMAL);

			// Draw app name
			u16 *title = app->name.text;

			// Measure text width
			RECT testRect = {0};
			DrawTextW(DrawingContext, title, -1, &testRect, DT_CALCRECT | DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
			i32 textWidth = (testRect.right - testRect.left) + 2; // Hmm, measured size seems a teensy bit too small, so +2

			// DrawTextW uses a destination rect for drawing
			RECT textRect = {0};
			textRect.left = left;
			textRect.right = right;
			textRect.bottom = rect.bottom;

			i32 widthDiff = textWidth - (textRect.right - textRect.left);
			if (widthDiff > 0) {
				// textRect is too small for text, grow the rect
				textRect.left -= (i32)ceil(widthDiff / 2.0);
				textRect.right += (i32)floor(widthDiff / 2.0);
			}

			textRect.bottom -= ICON_PAD; // *** this lifts the app text up a little ***

			// If the app name, when centered, extends past window bounds, adjust label position to be inside window bounds
			if (textRect.right > rect.right) {
				textRect.left -= (textRect.right - rect.right) + ICON_PAD;
				textRect.right = rect.right - ICON_PAD;
			}
			if (textRect.left < 0) {
				textRect.right += ICON_PAD + abs(textRect.left);
				textRect.left   = ICON_PAD + 0;
			}

			///* dbg */ FillRect(DrawingContext, &textRect, CreateSolidBrush(RGB(255, 0, 0))); // Check the size of the text box
			DrawTextW(DrawingContext, title, -1, &textRect, DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
		}
	}
}

static void _DisplaySwitcher(handle hwnd, u32 msg, u64 eventid, u32 time)
{
	// TimerProc used by DisplaySwitcher
	// Call DisplaySwitcher with 0 delay when timer activates
	void DisplaySwitcher(u32 delay);
	DisplaySwitcher(0);
}
static void DisplaySwitcher(u32 delay)
{
	KillTimer(Switcher, /*timerid*/ 1);
	if (delay > 0) {
		SetTimer(Switcher, /*timerid*/ 1, delay, _DisplaySwitcher); // _DisplaySwitcher calls DisplaySwitcher with 0 delay
		return;
	}
	ShowWindow(Switcher, SW_SHOWNA);
	//LockSetForegroundWindow
}
static void CancelSwitcher(bool restore)
{
	Print(L"cancel/close\n");
	KillTimer(Switcher, /*timerid*/ 1);
	HideWindow(Switcher);
	if (restore) {
		// TODO Check that the restore window is not the second most recently
		//      activated window (after my own switcher window), so that I
		//      don't needlessly switch to the window that will naturally
		//      become activated once my switcher window hides
		DisplayWindow(RestorableWindow);
	}
	// Reset selection
	SelectedApp = null;
	SelectedWindow = null;
	RestorableWindow = null;
}

static void QuitWindows(handle hwnd) // Alternative to TerminateWindowProcess
{
	if (AppsCount <= 0) {
		return;
	}
	struct app *app = null;
	handle *window = null;
	for (int i = 0; i < AppsCount; i++) {
		app = &Apps[i];
		for (int j = 0; j < app->windowsCount; j++) {
			if (app->windows[j] == hwnd) {
				Print(L"quit app %s\n", app->name);
				for (int k = 0; k < app->windowsCount; k++) {
					QuitWindow(app->windows[k]);
				}
				return;
			}
		}
	}
}

static bool SetKey(u32 key, bool down)
{
	#ifndef CHAR_BIT
	#define CHAR_BIT 8
	#endif

	bool wasdown = Keyboard[key/CHAR_BIT] & (1 << (key % CHAR_BIT)); // getbit
	if (down) {
		Keyboard[key/CHAR_BIT] |= (1 << (key % CHAR_BIT)); // setbit1
	} else {
		Keyboard[key/CHAR_BIT] &= ~(1 << (key % CHAR_BIT)); // setbit0
	}
	return wasdown && down; // Return true if this was a key repeat
}
static i64 KeyboardHookProcedure(int code, u64 wparam, i64 lparam)
{
	if (code < 0) { // "If code is less than zero, the hook procedure must pass the message to..." blah blah blah read the docs
		return CallNextHookEx(null, code, wparam, lparam);
	}

	KBDLLHOOKSTRUCT *kbd = lparam;
	u32 keyCode = kbd->vkCode;
	bool keyDown = !(bool)(kbd->flags & LLKHF_UP);
	bool injected = (kbd->flags & LLKHF_INJECTED) == LLKHF_INJECTED;

	if (injected) {
		Print(L"INJECTED INPUT\n");
		//return CallNextHookEx(null, code, wparam, lparam);
	}

	if (!SelectedWindow) {

		bool keyRepeat = SetKey(keyCode, keyDown); // Manually track key repeat
		bool shiftHeld = IsKeyDown(VK_LSHIFT) || IsKeyDown(VK_RSHIFT) || IsKeyDown(VK_SHIFT);

		// ========================================
		// Activation
		// ========================================

		bool key1Down = keyCode == Config.hotkeyForApps.key && keyDown;
		bool key2Down = keyCode == Config.hotkeyForWindows.key && keyDown;
		bool mod1Held = IsKeyDown(Config.hotkeyForApps.mod);
		bool mod2Held = IsKeyDown(Config.hotkeyForWindows.mod);

		// First Alt-Tab
		if (key1Down && mod1Held && Config.switchApps) {
			StealFocus();
			UpdateApps();
			SelectFirstWindow(); // Initialize selection
			if (*SelectedWindow == GetCoreWindow(GetForegroundWindow())) { // Don't select next when on blank desktop
				SelectNextWindow(true, shiftHeld, !Config.wrapbump || !keyRepeat);
			}
			if (Config.showSwitcherForApps) {
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before DisplaySwitcher
				RedrawSwitcher();
				DisplaySwitcher(Config.showSwitcherDelay);
			}
			if (Config.fastSwitchingForApps) {
				DisplayWindow(*SelectedWindow);
			}
			// Since I "own" this hotkey system-wide, I consume this message unconditionally
			// This will consume the key1Down event. Windows will not know that key1 was pressed
			goto consumeMessage;
		}
		// First Alt-Tilde/Backquote
		if (key2Down && mod2Held && Config.switchWindows) {
			StealFocus();
			UpdateApps();
			SelectFirstWindow(); // Initialize selection
			SelectNextWindow(false, shiftHeld, !Config.wrapbump || !keyRepeat);
			if (Config.showSwitcherForWindows) {
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before DisplaySwitcher
				RedrawSwitcher();
				DisplaySwitcher(Config.showSwitcherDelay);
			}
			if (Config.fastSwitchingForWindows) {
				DisplayWindow(*SelectedWindow);
			}
			// Since I "own" this hotkey system-wide, I consume this message unconditionally
			// This will consume the key2Down event. Windows will not know that key2 was pressed
			goto consumeMessage;
		}

	} else { // if (SelectedWindow) {

		// ========================================
		// Deactivation
		// ========================================

		bool mod1Up = keyCode == Config.hotkeyForApps.mod && !keyDown;
		bool mod2Up = keyCode == Config.hotkeyForWindows.mod && !keyDown;

		if (mod1Up || mod2Up) {
			// Alt keyup - switch to selected window
			// Always pass/never consume mod key up, because:
			// 1) cmdtab never hooks mod keydowns
			// 2) cmdtab hooks mod keyups, but always passes it through on pain of getting a stuck Alt key because of point 1
			StealFocus();
			DisplayWindow(*SelectedWindow);
			CancelSwitcher(false);
			goto passMessage; // See note above on "Alt keyup" on why I don't consume this message
		} else {

			bool keyRepeat = SetKey(keyCode, keyDown); // Manually track key repeat
			bool shiftHeld = IsKeyDown(VK_LSHIFT) || IsKeyDown(VK_RSHIFT) || IsKeyDown(VK_SHIFT);

			// ========================================
			// Navigation
			// ========================================

			bool key1Down = keyCode == Config.hotkeyForApps.key && keyDown;
			bool key2Down = keyCode == Config.hotkeyForWindows.key && keyDown;
			bool prevDown = (keyCode == VK_UP || keyCode == VK_LEFT) && keyDown;
			bool nextDown = (keyCode == VK_DOWN || keyCode == VK_RIGHT) && keyDown;

			// Subsequent Alt-Tab - next app (hold Shift for previous)
			// Also arrow keys, left and right
			if (key1Down || (nextDown || prevDown)) {
				SelectNextWindow(true, shiftHeld || prevDown, !Config.wrapbump || !keyRepeat);
				if (Config.showSwitcherForApps) {
					ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before DisplaySwitcher
					RedrawSwitcher();
					DisplaySwitcher(Config.showSwitcherDelay);
				}
				if (Config.fastSwitchingForApps) {
					PreviewWindow(*SelectedWindow);
				}
				// Since I "own" this hotkey system-wide, I consume this message unconditionally
				// This will consume the key1Down event. Windows will not know that key1 was pressed
				goto consumeMessage;
			}

			// Subsequent Alt-Tilde/Backquote - next window (hold Shift for previous)
			// TODO Also arrow keys, up and down
			if (key2Down) {
				SelectNextWindow(false, shiftHeld, !Config.wrapbump || !keyRepeat);
				if (Config.showSwitcherForWindows) {
					ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before DisplaySwitcher
					RedrawSwitcher();
					DisplaySwitcher(Config.showSwitcherDelay);
				}
				if (Config.fastSwitchingForWindows) {
					PreviewWindow(*SelectedWindow);
				}
				// Since I "own" this hotkey system-wide, I consume this message unconditionally
				// This will consume the key2Down event. Windows will not know that key2 was pressed
				goto consumeMessage;
			}

			// ========================================
			// Deactivation
			// ========================================

			bool escDown = keyCode == VK_ESCAPE && keyDown;
			bool enterDown = keyCode == VK_RETURN && keyDown;

			// Alt-Enter - switch to selected window
			if (enterDown) {
				DisplayWindow(*SelectedWindow);
				CancelSwitcher(false);
				// Since our swicher is active, I "own" this hotkey, so I consume this message
				// This will consume the enterDown event. Windows will not know that Enter was pressed
				goto consumeMessage;
			}
			// Alt-Esc - cancel switching and restore initial window
			if (escDown) { // BUG There's a bug here if mod1Held and mod2Held aren't both the same key. In that case I'll still pass through the key event, even though I should own it since it's not Alt-Esc
				CancelSwitcher(Config.restoreOnCancel);
				// Since our swicher is active, I "own" this hotkey, so I consume this message
				// This will consume the escDown event. Windows will not know that Escape was pressed
				goto consumeMessage;
			}

			// ========================================
			// Extended window management
			// ========================================

			bool keyF4Down  = keyCode == VK_F4 && keyDown;
			bool keyQDown   = keyCode == 'Q' && keyDown;
			bool keyWDown   = keyCode == 'W' && keyDown;
			bool deleteDown = keyCode == VK_DELETE && keyDown;
			bool keyMDown   = keyCode == 'M' && keyDown;
			bool keyHDown   = keyCode == 'H' && keyDown;
			bool keyBDown   = keyCode == 'B' && keyDown;

			// Alt-F4 - quit cmdtab
			if (keyF4Down) {
				QuitWindow(Switcher);
				goto consumeMessage;
			}
			// Alt-Q - close all windows of selected application
			if (keyQDown) {
				TerminateWindowProcess(*SelectedWindow);
				if (SelectedApp == &Apps[AppsCount-1] && AppsCount > 1) {
					SelectedApp--;
				}
				//QuitWindows(*SelectedWindow);
				UpdateApps();
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before DisplaySwitcher
				RedrawSwitcher();
				goto consumeMessage;
			}
			// Alt-W or Alt-Delete - close selected window
			if (keyWDown || deleteDown) {
				QuitWindow(*SelectedWindow);
				UpdateApps();
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before DisplaySwitcher
				RedrawSwitcher();
				goto consumeMessage;
			}
			// Alt-M - minimize selected window
			if (keyMDown) {
				MinimizeWindow(*SelectedWindow);
				goto consumeMessage;
			}
			// Alt-H - hide selected window
			if (keyHDown) {
				//hide(*SelectedWindow); // WARNING Can't use this because we filter out hidden windows
				goto consumeMessage;
			}
			// Alt-B - display app name and window class to user, to add in blacklist
			if (keyBDown) {
				ReportWindowHandle(Switcher, *SelectedWindow);
				CancelSwitcher(false);
				goto consumeMessage;
			}

			// ========================================
			// Debug
			// ========================================

			bool keyF11Down = keyCode == VK_F11 && keyDown;
			bool keyF12Down = keyCode == VK_F12 && keyDown;

			if (keyF12Down) {
				Debug(L"debug hotkey");
			}
			if (keyF11Down) {
				PrintApps();
			}

			// ========================================
			// Hook management
			// ========================================

			// Pass through modifiers so they don't get stuck
			if (keyCode == VK_CONTROL || keyCode == VK_LCONTROL || keyCode == VK_RCONTROL ||
				keyCode == VK_MENU    || keyCode == VK_LMENU    || keyCode == VK_RMENU    ||
				keyCode == VK_SHIFT   || keyCode == VK_LSHIFT   || keyCode == VK_RSHIFT   ||
				keyCode == VK_CAPITAL) {
				Print(L"modifier passthrough\n");
				goto passMessage;
			}
			// Other passthrough
			if (keyCode == VK_SNAPSHOT) {
				goto passMessage;
			}
			// Input sink - consume keystrokes when activated
			if (true) {
				u16 keyName[32] = {0};
				int length = GetKeyNameTextW(kbd->scanCode << 16, keyName, countof(keyName));
				Print(L"input sink %s %s\n", keyName, keyDown ? L"down" : L"up");
				goto consumeMessage;
			}
		}
	}

	passMessage:
		//print(L"allow next hook\n");
		return CallNextHookEx(null, code, wparam, lparam);
	consumeMessage:
		// By returning a non-zero value from the hook procedure, the message
		// is consumed and does not get passed to the target window
		//print(L"block key event\n");
		return 1;
}

static void OnWindowCreate(void)
{

}
static void OnWindowPaint(void)
{
	// Double buffered drawing:
	// I draw off-screen in-memory to 'DrawingContext' whenever changes occur,
	// and whenever I get the WM_PAINT message I just blit the
	// off-screen buffer to screen ("screen" means the 'hdc' returned by
	// BeginPaint passed our 'hwnd')
	PAINTSTRUCT ps = {0};
	handle context = BeginPaint(Switcher, &ps);
	if (ps.fErase) {
	}
	BitBlt(context, 0, 0, DrawingRect.right - DrawingRect.left, DrawingRect.bottom - DrawingRect.top, DrawingContext, 0, 0, SRCCOPY);
	//int scale = 1.0;
	//SetStretchBltMode(DrawingContext, HALFTONE);
	//StretchBlt(hdc, 0, 0, gdi_width, gdi_height, DrawingContext, 0, 0, gdi_width * scale, gdi_height * scale, SRCCOPY);
	EndPaint(Switcher, &ps);
	ReleaseDC(Switcher, context);
}
static void OnWindowFocusChange(bool focused)
{
	// Cancel on mouse clicking outside window
	if (focused) {
		Print(L"switcher got focus\n");
		//return 0;
		//synckeys();
	} else {
		// This rarely happens atm with the current implementation
		// because I don't set the switcher as foreground window when
		// showing! I just show the window and rely on being topmost.
		// But the user can click the switcher window and make it the
		// foreground window, in which case it will get this message
		// when the foreground window changes again.
		// I'm not sure if the implementation will remain like this
		// (i.e. not becoming foreground)--it's a little unorthodox.
		// But for now that's how it is.
		Print(L"switcher lost focus\n");
		//return 1; // Nu-uh!
		//cancel(false); // NOTE See comments in cancel
	}
}
static void OnWindowMouseMove(int x, int y)
{
	MouseX = x; // GetMessagePos
	MouseY = y;
	//print(L"x%i y%i\n", MouseX, MouseY);
}
static void OnWindowMouseLeave(void)
{
	MouseX = 0;
	MouseY = 0;
	//print(L"x%i y%i\n", MouseX, MouseY);
}
static void OnWindowClose(void)
{
	CancelSwitcher(false);
	if (Ask(Switcher, L"Quit cmdtab?")) {
		DestroyWindow(Switcher);//return DefWindowProcW(hwnd, message, wparam, lparam); // Calls DestroyWindow
	}
}
static void OnShellWindowActivated(handle hwnd)
{
	PrintWindowHandle(hwnd);
	if (!IsAltTabWindow(hwnd)) {
		Print(L"  ignored\n");
	}
	if (GetCoreWindow(hwnd) != hwnd) {
		Print(L"  uwp: ");
		PrintWindowHandle(GetCoreWindow(hwnd));
	}
	AddToHistory(hwnd);
}
static i64 WindowProcedure(handle hwnd, u32 message, u64 wparam, i64 lparam)
{
	static u32 ShellHookMessage = WM_NULL;
	switch (message) {
		case WM_CREATE:
			ShellHookMessage = RegisterWindowMessageW(L"SHELLHOOK");
			OnWindowCreate();
			break;
		//case WM_ERASEBKGND:
		//	return 1;
		case WM_PAINT:
			OnWindowPaint();
			break;
		case WM_ACTIVATEAPP: // WM_ACTIVATE
			OnWindowFocusChange(wparam != WA_INACTIVE);
			break;
		case WM_MOUSEMOVE:
			OnWindowMouseMove(LOWORD(lparam), HIWORD(lparam));
			TrackMouseEvent(&(TRACKMOUSEEVENT){.cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = hwnd});
			break;
		case WM_MOUSELEAVE:
			OnWindowMouseLeave();
			break;
		case WM_CLOSE:
			OnWindowClose();
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		//case WM_QUIT:
		//    break;
		default: {
			if (message == ShellHookMessage) {
				switch (wparam) {
					//case HSHELL_WINDOWCREATED: {
					//	Print(L"CREATE window ");
					//	handle hwnd2 = lparam;
					//	PrintWindowHandle(hwnd2);
					//	if (!IsAltTabWindow(hwnd2)) {
					//		Print(L"  ignored\n");
					//	}
					//	if (GetCoreWindow(hwnd2) != hwnd2) {
					//		Print(L"  uwp: ");
					//		PrintWindowHandle(GetCoreWindow(hwnd2));
					//	}
					//	//OnNewWindow((HWND)LParam);
					//	break;
					//}
					//case HSHELL_WINDOWDESTROYED: {
					//	Print(L"CLOSED window ");
					//	handle hwnd2 = lparam;
					//	PrintWindowHandle(hwnd2);
					//	if (!IsAltTabWindow(hwnd2)) {
					//		Print(L"  ignored\n");
					//	}
					//	if (GetCoreWindow(hwnd2) != hwnd2) {
					//		Print(L"  uwp: ");
					//		PrintWindowHandle(GetCoreWindow(hwnd2));
					//	}
					//	//OnDestroyWindow((HWND)LParam);
					//	break;
					//}
					case HSHELL_WINDOWACTIVATED:
					case HSHELL_FLASH:
					case HSHELL_RUDEAPPACTIVATED:
						break; // Disable
						if (wparam == HSHELL_WINDOWACTIVATED) {
							Print(L"ACTIVATED window ");
						} else if (wparam == HSHELL_FLASH) {
							Print(L"FLASH window ");
						} else if (wparam == HSHELL_RUDEAPPACTIVATED) {
							Print(L"RUDEAPP ACTIVATED window ");
						}
						OnShellWindowActivated((handle)lparam);
						break;
					default:
						Print(L"HSHELL msg %u\n", (u32)wparam);
						break;
				}
			}
			return DefWindowProcW(hwnd, message, wparam, lparam);
		}
	}
	return 0;
}

static void EventsHookProcedure(handle hook, u32 event, handle hwnd, i32 objectid, i32 childid, u32 threadid, u32 timestamp)
{
	if (event == EVENT_SYSTEM_FOREGROUND ||
		event == EVENT_OBJECT_UNCLOAKED) {
		if (event == EVENT_SYSTEM_FOREGROUND) {
			Print(L"FOREGROUND window ");
		} else if (event == EVENT_OBJECT_UNCLOAKED) {
			Print(L"UNCLOAKED window ");
		}
		OnShellWindowActivated(hwnd);
	}
}

static bool HasAutorunLaunchArgument(u16 *args)
{
	return !wcscmp(args, L"--autorun");
}
static void AskAutorun(void)
{
	SetAutorun(Ask(Switcher, L"Start cmdtab automatically?\nRelaunch cmdtab.exe to change your mind."), L"cmdtab", L"--autorun");
}
static bool AlreadyRunning(handle *mutex)
{
	*mutex = CreateMutexW(null, true, L"cmdtabMutex");
	return GetLastError() == ERROR_ALREADY_EXISTS;
}
static void QuitSecondInstance(void)
{
	MessageBoxW(null, L"cmdtab is already running.", L"cmdtab", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
	ExitProcess(0);
}

static void InitConfig(void)
{
	Config = (struct config){
		// The default key2 is scan code 0x29 (hex) / 41 (decimal)
		// This is the key that is physically below Esc, left of 1 and above Tab
		//
		// WARNING!
		// VK_OEM_3 IS NOT SCAN CODE 0x29 FOR ALL KEYBOARD LAYOUTS.
		// YOU MUST CALL THE FOLLOWING DURING RUNTIME INITIALIZATION:
		// `.key2 = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX)`

		// Hotkeys
		.hotkeyForApps =    { VK_LMENU, VK_TAB },
		.hotkeyForWindows = { VK_LMENU, VK_OEM_3 }, // Must be updated during runtime. See note above.
		// Behavior
		.switchApps              = true,
		.switchWindows           = true,
		.fastSwitchingForApps    = false, // default false
		.fastSwitchingForWindows = true,
		.showSwitcherForApps     = true,
		.showSwitcherForWindows  = false,
		.showSwitcherDelay       = 0,
		.wrapbump                = true,
		.ignoreMinimized         = false,
		.restoreOnCancel         = false,
		// Appearance
		.darkmode                = false,
		.switcherHeight          = 128,
		.switcherHorzMargin      =  24,
		.switcherVertMargin      =  32,
		.iconWidth               =  64,
		.iconHorzPadding         =   8,
		// Blacklist
		.blacklist = {
			{ null,                       L"ApplicationFrameWindow" },
			//{ L"ApplicationFrameHost",  null },
			//{ L"ApplicationFrameHost",  L"ApplicationFrameWindow" },
			//{ null,                     L"Windows.UI.Core.CoreWindow" },
			//{ null,                     L"Xaml_WindowedPopupClass" },
			{ L"SearchHost",              L"Windows.UI.Core.CoreWindow" },
			{ L"StartMenuExperienceHost", L"Windows.UI.Core.CoreWindow" },
			//{ L"explorer",              L"ApplicationFrameWindow" },
			//{ L"explorer",              L"ApplicationManager_DesktopShellWindow" },
			//{ L"TextInputHost",         null },
		},
	};
	// Initialize runtime-dependent settings. This one is dependent on user's current kbd layout
	Config.hotkeyForWindows.key = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX);
	Config.darkmode = IsDarkModeEnabled();

	Print(L"darkmode %s\n", Config.darkmode ? L"YES" : L"NO");
}
static void InstallKeyboardHook(handle *hook)
{
	// Install (low level) keyboard hook
	*hook = SetWindowsHookExW(WH_KEYBOARD_LL, (HOOKPROC)KeyboardHookProcedure, null, 0);
}
static void InstallEventsHook(handle *hook, handle *hook2)
{
	// Install window event hook for one event: foreground window changes. Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20130930-00/?p=3083
	*hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, null, EventsHookProcedure, 0, 0, WINEVENT_OUTOFCONTEXT);
	// and EVENT_OBJECT_CLOAKED? EVENT_OBJECT_HOSTEDOBJECTSINVALIDATED?
	*hook2 = SetWinEventHook(EVENT_OBJECT_UNCLOAKED, EVENT_OBJECT_UNCLOAKED, null, EventsHookProcedure, 0, 0, WINEVENT_OUTOFCONTEXT);
}
static void UninstallKeyboardHook(handle hook)
{
	UnhookWindowsHookEx(hook);
}
static void UninstallEventsHook(handle hook, handle hook2)
{
	UnhookWinEvent(hook);
	UnhookWinEvent(hook2);
}

static void CreateSwitcherWindow(handle instance)
{
	WNDCLASSEXW wcex = {0};
	wcex.cbSize = sizeof wcex;
	//wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)WindowProcedure;
	wcex.hInstance = instance;
	wcex.lpszClassName = L"cmdtabSwitcher";
	Switcher = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, RegisterClassExW(&wcex), null, 0, 0, 0, 0, 0, null, null, instance, null);

	// Clear all window styles for very plain window
	SetWindowLongW(Switcher, GWL_STYLE, 0);

	// Set the *title bar* to dark mode
	// For dark mode details, see: https://stackoverflow.com/a/70693198/659310
	//BOOL darktitlebar = TRUE;
	//DwmSetWindowAttribute(*window, DWMWA_USE_IMMERSIVE_DARK_MODE, &darktitlebar, sizeof darktitlebar);

	// Rounded window corners
	DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
	DwmSetWindowAttribute(Switcher, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners);
}
static void RunMessageLoop()
{
	// Start msg loop for thread
	for (MSG msg; GetMessageW(&msg, Switcher, 0, 0) > 0;) DispatchMessageW(&msg); // Not handling -1 errors. Whatever
}

static void QuitCmdTab(handle mutex)
{
	if (mutex) {
		ReleaseMutex(mutex);
		//CloseHandle(hMutex);
	}
	Print(L"cmdtab quit\n");
	bool hasLeaks = _CrtDumpMemoryLeaks();
	ExitProcess(hasLeaks);
}
static void RunCmdTab(handle instance, u16 *args)
{
	handle mutex;
	//handle mainWindow;
	handle keyboardHook, eventHook1, eventHook2;
	if (!HasAutorunLaunchArgument(args)) {
		AskAutorun();
	}
	if (AlreadyRunning(&mutex)) {
		QuitSecondInstance();
	}
	InitConfig();
	CreateSwitcherWindow(instance);
	InstallKeyboardHook(&keyboardHook);
	InstallEventsHook(&eventHook1, &eventHook2);
	//RegisterShellHookWindow(Switcher);
	RunMessageLoop(Switcher);
	//DeregisterShellHookWindow(Switcher);
	//DestroyWindow(Switcher);
	UninstallKeyboardHook(keyboardHook);
	UninstallEventsHook(eventHook1, eventHook2);
	QuitCmdTab(mutex);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	//#ifdef _DEBUG
	//	FILE *conout = null;
	//	AllocConsole();
	//	freopen_s(&conout, "CONOUT$", "w", stdout);
	//#endif

	RunCmdTab(hInstance, lpCmdLine);
	return 0;
}
