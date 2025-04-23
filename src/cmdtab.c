#include <math.h>

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _CRTDBG_MAP_ALLOC
#define COBJMACROS

//#undef _DEBUG

#define NOGDICAPMASKS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX

#include <windows.h>
#include "crtdbg.h"
#include "shellapi.h" // Only used by StringFileName for PathFindFileNameW?
#include "shlwapi.h"
#include "dwmapi.h"
#include "pathcch.h"
#include "strsafe.h"
#include <initguid.h>
#include "commoncontrols.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib") // Only used by StringFileName for PathFindFileNameW?
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "pathcch.lib")
#pragma comment(lib, "version.lib")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")

//==============================================================================
// Utility belt
//==============================================================================

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

static bool StringsAreEqual(string *s1, string *s2)
{
	// Reverse compare strings
	// Compare in reverse since this function is mostly used to compare file
	// paths, which are often unique yet have long identical prefixes
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
	return s1->length >= s2->length && !memcmp(s1->text + s1->length - s2->length, s2->text, s2->length);
}

static string StringFileName(string *filepath, bool extension)
{
	// aka. basename
	string name = {0};
	name.ok = SUCCEEDED(StringCchCopyNW(name.text, countof(name.text), PathFindFileNameW(filepath->text), countof(name.text)));
	if (!extension) {
		name.ok = name.ok && SUCCEEDED(PathCchRemoveExtension(name.text, countof(name.text)));
	}
	name.ok = name.ok && SUCCEEDED(StringCchLengthW(name.text, countof(name.text), &name.length));
	return name;
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

static void Debug(u16 *fmt, ...)
{
	#ifdef _DEBUG
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	OutputDebugStringW(buffer);
	__debugbreak();
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

static void Log(u16 *fmt, ...)
{
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	OutputDebugStringW(buffer);
}

static void Error(handle hwnd, u16 *fmt, ...)
{
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	OutputDebugStringW(buffer);
	SetForegroundWindow(hwnd);
	MessageBoxW(hwnd, buffer, L"cmdtab", MB_OK | MB_ICONERROR | MB_TASKMODAL);
}

static bool Ask(handle hwnd, u16 *fmt, ...)
{
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	SetForegroundWindow(hwnd);
	return MessageBoxW(hwnd, buffer, L"cmdtab", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL) == IDYES;
}

static string GetWindowClass(handle hwnd)
{
	string class = {0};
	class.length = GetClassNameW(hwnd, class.text, countof(class.text));
	class.ok = (class.length > 0);
	return class;
}

static BOOL CALLBACK _GetCoreWindow(HWND child, LPARAM lparam)
{
	// EnumChildProc used by GetCoreWindow
	// Set inout 'lparam' to hosted core window
	string class = GetWindowClass(child);
	if (StringsAreEqual(&class, &UWPCoreWindowClass)) {
		*(handle *)lparam = child;
		return false;
	} else {
		return true; // Return true to continue EnumChildWindows
	}
}

static handle GetCoreWindow(handle hwnd)
{
	string class = GetWindowClass(hwnd);
	if (StringsAreEqual(&class, &UWPAppHostClass)) {
		handle hwnd2 = hwnd;
		EnumChildWindows(hwnd, _GetCoreWindow, (LPARAM)&hwnd2); // _GetCoreWindow sets 'hwnd2' to hosted core window, if any
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
	if (!IsWindowVisible(hwnd)) {
		return false;
	}
	int cloaked = 0;
	DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked);
	if (cloaked) {
		return false;
	}
	if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) {
		return true;
	}
	if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE ||
		GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
		return false;
	}
	return true;
}

static string GetExePath(handle hwnd)
{
	string path = {0};
	ULONG pid;
	GetWindowThreadProcessId(hwnd, &pid);
	handle process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid);
	if (!process || process == INVALID_HANDLE_VALUE) {
		Print(L"WARNING couldn't open process with pid: %u\n", pid);
		return path;
	}
	ULONG length = countof(path.text);
	path.ok = QueryFullProcessImageNameW(process, 0, path.text, &length);
	CloseHandle(process);
	if (!path.ok) {
		Print(L"WARNING couldn't get exe path for process with pid: %u\n", pid);
		return path;
	}
	path.length = length;
	return path;
}

static string GetWindowTitle(handle hwnd)
{
	string title = {0};
	title.length = GetWindowTextW(hwnd, title.text, countof(title.text));
	title.ok = (title.length > 0);
	return title;
}

static void PrintWindowX(handle hwnd) // PrintWindow already taken
{
	#ifdef _DEBUG
	if (hwnd) {
		string filepath = GetExePath(hwnd);
		if (!filepath.ok) return;
		string filename = StringFileName(&filepath, false);
		string class = GetWindowClass(hwnd);
		string title = GetWindowTitle(hwnd);
		u16 *wsShowState        = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_MINIMIZE) ? L" (minimized) " : 
			(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_MAXIMIZE) ? L" (maximized) " : L" ";
		u16 *wsChild            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) ? L"child" : L"parent";
		u16 *wsVisible          = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_VISIBLE) ? L", visible" : L", hidden";
		u16 *exAppWindow        = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) ? L", appwindow" : L"";
		u16 *exNoActivate       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) ? L", noactivate" : L"";
		u16 *exToolWindow       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ? L", toolwindow" : L"";
		u16 *exTopMost          = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) ? L", topmost" : L"";
		u16 *wsDisabled         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DISABLED) ? L", disabled" : L", enabled";
		u16 *wsPopup            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUP) ? L", popup" : L"";
		u16 *wsOverlapped       = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPED) ? L", tiled/overlapped" : L"";
		u16 *wsDlgFrame         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DLGFRAME) ? L", dlgframe" : L"";
		u16 *exDlgModalFrame    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_DLGMODALFRAME) ? L", dlgmodalframe" : L"";
		u16 *exLayered          = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) ? L", layered" : L"";
		u16 *exComposited       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_COMPOSITED) ? L", composited" : L"";
		u16 *exPaletteWindow    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_PALETTEWINDOW) ? L", palettewindow" : L"";
		u16 *exOverlappedWindow = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_OVERLAPPEDWINDOW) ? L", exoverlappedwindow" : L"";
		Print(L"%p { %s, %s }%s\"%.32s\"", hwnd, filename.text, class.text, wsShowState, title.text);
		Print(L" (%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s)\n",
			wsChild, wsVisible, exAppWindow, exNoActivate, exToolWindow, exTopMost,
			wsDisabled, wsPopup, wsOverlapped, wsDlgFrame, exDlgModalFrame, exLayered, exComposited, exPaletteWindow, exOverlappedWindow);
	} else {
		Print(L"null hwnd\n");
	}
	#else
	Print(L"%p\n", hwnd);
	#endif
}

static string GetAppName(string *filepath)
{
	// Query version info for pretty application name. See cmdtab's own res/cmdtab.rc file for an idea of the data that is queried.
	
	string name = {0};

	struct {
		void *data;
		u32 size;
	} versionInfo;

	if (!(versionInfo.size = GetFileVersionInfoSizeW(filepath->text, null))) {
		goto abort;
	}
	if (!(versionInfo.data = malloc(versionInfo.size))) {
		goto abort;
	}
	if (!GetFileVersionInfoW(filepath->text, 0, versionInfo.size, versionInfo.data)) {
		goto free;
	}

	struct {
		u16 language;
		u16 codepage;
	} *translation;
	
	if (!VerQueryValueW(versionInfo.data, L"\\VarFileInfo\\Translation", (void *)&translation, null)) {
		goto free;
	}

	u16 key[MAX_PATH];

	struct {
		u16 *text;
		u32 length;
	} value;

	// Prefer the pretty application name in the "FileDescription" field
	StringCchPrintfW(key, countof(key), L"\\StringFileInfo\\%04x%04x\\FileDescription", translation[0].language, translation[0].codepage);

	if (VerQueryValueW(versionInfo.data, key, (void *)&value.text, &value.length) && wcslen(value.text) > 0) {
		goto success;
	}

	// Fall back to "ProductName" if "FileDescription" is empty. Example: Github Desktop
	StringCchPrintfW(key, countof(key), L"\\StringFileInfo\\%04x%04x\\ProductName", translation[0].language, translation[0].codepage);

	if (VerQueryValueW(versionInfo.data, key, (void *)&value.text, &value.length) && wcslen(value.text) > 0) {
		goto success;
	}

	// Fall back to base filename if "ProductName" is empty
	name = StringFileName(filepath, false);
	goto free;

	success:
		name.length = value.length < countof(name.text) ? value.length : countof(name.text);
		name.ok = SUCCEEDED(StringCchCopyNW(name.text, countof(name.text)-1, value.text, countof(name.text)-1));
	free:
		free(versionInfo.data);
	abort:
		return name;
}

static handle GetAppIcon(string *filepath)
{
	//IsImmersiveProcess();
	//GetApplicationUserModelId(); // appmodel.h
	
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

	//if (FAILED(CoInitialize(null))) {
	//	return null;
	//}
	IImageList *list = {0};
	if (FAILED(SHGetImageList(SHIL_JUMBO, &IID_IImageList, (void **)&list))) {
		return null;
	}
	SHFILEINFOW info = {0};
	if (!SHGetFileInfoW(filepath->text, 0, &info, sizeof info, SHGFI_SYSICONINDEX)) { // 2nd arg: -1, 0 or FILE_ATTRIBUTE_NORMAL
		Debug(L"suspicious SHGetFileInfoW return"); // SHGetFileInfoW has special return with SHGFI_SYSICONINDEX flag, which I think should never fail (docs unclear?)
		return null;
	}
	HICON icon = {0};
	IImageList_GetIcon(list, info.iIcon, ILD_TRANSPARENT, &icon);
	IImageList_Release(list);
	return icon; // Caller is responsible for calling DestroyIcon
}

static bool IsDarkModeEnabled(void)
{
	ULONG value;
	ULONG size = sizeof value;
	if (!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUsesLightTheme", RRF_RT_DWORD, null, &value, &size)) {
		return !value; // 'value' is true for light mode
	} else {
		return true; // Default to dark mode
	}
}

static u32 GetAccentColor(void)
{
	ULONG value;
	ULONG size = sizeof value;
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
	target.length = wcslen(target.text);
	HKEY regkey;
	i32 success; // BUG Not checking 'success' below
	if (enabled) {
		success = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, null, 0, KEY_SET_VALUE, null, &regkey, null);
		success = RegSetValueExW(regkey, keyname, 0, REG_SZ, (UCHAR *)target.text, target.length * sizeof *target.text); // TODO Oof, that's a nasty cast
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

static int ReportWindowHandle(handle hwnd, handle target)
{
	// Tell user the executable name and class so they can add it to the blacklist
	string class = GetWindowClass(target);
	string path = GetExePath(target);
	string name = StringFileName(&path, false);

	u16 buffer[2048];
	StringCchPrintfW(buffer, countof(buffer)-1, L"Add to blacklist?\n\nName:\t%s\nClass:\t%s\n\nPress Ctrl+C to copy this message.\n\nNote: Blacklist not working yet, it does nothing, TODO!", name.text, class.text);
	SetForegroundWindow(hwnd);
	return MessageBoxW(hwnd, buffer, L"cmdtab", MB_OK | MB_ICONASTERISK | MB_TASKMODAL) == IDYES;
}

static void ReceiveLastInputEvent(void)
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

static void ShowWindowX(handle hwnd) // ShowWindow already taken
{
	if (GetForegroundWindow() != hwnd) {
		if (IsIconic(hwnd)) {
			ShowWindow(hwnd, SW_RESTORE);
		} else {
			ShowWindow(hwnd, SW_SHOWNA);
		}
		if (SetForegroundWindow(GetAppHost(hwnd))) {
			Print(L"switch to ");
			PrintWindowX(hwnd);
		} else {
			PrintWindowX(hwnd);
			Error(null/*Switcher*/, L"ERROR couldn't switch to %s", GetWindowTitle(hwnd));
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
	ShowWindowX(hwnd);
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

static void CloseWindowX(handle hwnd) // CloseWindow already taken
{
	Print(L"close window ");
	PrintWindowX(hwnd);
	PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

static void TerminateWindowProcess(handle hwnd)
{
	ULONG pid;
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
	bool wrapbump;
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
		u16 *filename;          // Filename without file extension, ex. "explorer". Can be null.
		u16 *windowClass;       // Window class name. Can be null.
	} blacklist[32];
};

struct app {
	string path;                // Full path of app executable.
	string name;                // Product description/name, or filename without extension as fallback.
	handle icon;                // Big app icon.
	handle windows[64];         // App windows that can be switched to.
	size windowsCount;          // Number of elements in 'windows' array.
};

static struct app Apps[128];    // Apps that are displayed in switcher.
static size AppsCount;          // Number of elements in 'Apps' array.
static struct app *SelectedApp; // Pointer to one of the elements in 'Apps' array. The app for the 'SelectedWindow'.
static handle *SelectedWindow;  // Currently selected window in switcher. Non-NULL indicates switcher is active.
static handle RestorableWindow; // Whatever window was foreground when switcher was activated. Does not need to be in filtered 'Apps' array.
static handle History[128];     // Window activation history from Windows events. Deduplicated, so prior activations are moved to the front.
static size HistoryCount;       // Number of elements in 'History' array.
static handle Switcher;         // Handle for main window (i.e. switcher).
static handle DrawingContext;   // Drawing context for double-buffered drawing of main window.
static handle DrawingBitmap;    // Off-screen bitmap used for double-buffered drawing of main window.
static RECT DrawingRect;        // Size of the off-screen bitmap.
static u16 Keyboard[16];        // 256 bits to track key repeat for low-level keyboard hook.
static i32 MouseX, MouseY;      // Mouse position, for highlighting and clicking app icons.

static struct config Config = { // cmdtab settings
	// Hotkeys
	.hotkeyForApps =    { 0x38, 0x0F }, // Alt-Tab in scancodes, must be updated during runtime, see InitConfig
	.hotkeyForWindows = { 0x38, 0x29 }, // Alt-Tilde/Backquote in scancodes, must be updated during runtime, see InitConfig
	// Behavior
	.switchApps              = true,
	.switchWindows           = true,
	.fastSwitchingForApps    = false,
	.fastSwitchingForWindows = true,
	.showSwitcherForApps     = true,
	.showSwitcherForWindows  = false,
	.wrapbump                = true,
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
		{ L"SearchHost",              L"Windows.UI.Core.CoreWindow" },
		{ L"StartMenuExperienceHost", L"Windows.UI.Core.CoreWindow" },
	},
};

static void InitConfig(void)
{
	// Initialize runtime-dependent settings, like those dependent on user's current kbd layout
	// The default key2 is scan code 0x29 (hex) / 41 (decimal)
	// This is the key that is physically below Esc, left of 1 and above Tab
	Config.hotkeyForApps.mod = MapVirtualKeyW(Config.hotkeyForApps.mod, MAPVK_VSC_TO_VK_EX);
	Config.hotkeyForApps.key = MapVirtualKeyW(Config.hotkeyForApps.key, MAPVK_VSC_TO_VK_EX);
	Config.hotkeyForWindows.mod = MapVirtualKeyW(Config.hotkeyForWindows.mod, MAPVK_VSC_TO_VK_EX);
	Config.hotkeyForWindows.key = MapVirtualKeyW(Config.hotkeyForWindows.key, MAPVK_VSC_TO_VK_EX);
	Config.darkmode = IsDarkModeEnabled();
	Print(L"darkmode %s\n", Config.darkmode ? L"YES" : L"NO");
}

static void PrintApps(void)
{
	int windowsCount = 0;
	for (int i = 0; i < AppsCount; i++) {
		Print(L"%i ", i);
		struct app *app = &Apps[i];
		PrintWindowX(app->windows[0]);
		windowsCount += app->windowsCount;
		for (int j = 1; j < app->windowsCount; j++) {
			Print(L"  %i ", j);
			PrintWindowX(app->windows[j]);
		}
	}
	Print(L"%i apps %i windows, %i history:\n", AppsCount, windowsCount, HistoryCount);
	// And print History
	for (int i = 0; i < HistoryCount; i++) {
		Print(L"%i ", i);
		PrintWindowX(History[i]);
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
	// 
	if (History[0] == hwnd) {
		Print(L"(already first in history) ");
		Print(L"activate window %s", GetAppHost(hwnd) != hwnd ? L"(uwp) " : L"");
		PrintWindowX(hwnd);
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
		Print(L"(ignored) ");
		Print(L"activate window %s", GetAppHost(hwnd) != hwnd ? L"(uwp) " : L"");
		PrintWindowX(hwnd);
		return;
	}
	// Get module filepath, abort if failed (most likely access denied)
	string filepath = GetExePath(hwnd);
	if (!filepath.ok) {
		return; // Don't have to print error, since GetFilepath already does that
	}
	// Check window against user-defined blacklist
	string filename = StringFileName(&filepath, false);
	string class = GetWindowClass(hwnd);
	if (WindowIsBlacklisted(&filename, &class)) {
		Print(L"(blacklisted) ");
		Print(L"activate window %s", GetAppHost(hwnd) != hwnd ? L"(uwp) " : L"");
		PrintWindowX(hwnd);
		return;
	}

	// 'hwnd' is valid and should be added

	// Shift all hwnds down one position and then put 'hwnd' first
	if (HistoryCount < countof(History)) {
		memmove(&History[1], &History[0], sizeof History[0] * ++HistoryCount);
	} else {
		memmove(&History[1], &History[0], sizeof History[0] * HistoryCount);
	}
	History[0] = hwnd;
	Print(L"activate window %s", GetAppHost(hwnd) != hwnd ? L"(uwp) " : L"");
	PrintWindowX(hwnd);
}

static void AddToSwitcher(handle hwnd)
{
	// 1. Get hosted window in case of UWP host
	hwnd = GetCoreWindow(hwnd);
	// 2. Basic checks for eligibility in Alt-Tab
	if (!IsAltTabWindow(hwnd)) {
		Print(L"(ignored) ");
		Print(L"add window %s", GetAppHost(hwnd) != hwnd ? L"(uwp) " : L"");
		PrintWindowX(hwnd);
		return;
	}
	// 3. Get module filepath, abort if failed (most likely access denied)
	string filepath = GetExePath(hwnd);
	if (!filepath.ok) {
		return; // Don't have to print error, since GetFilepath already does that
	}
	// 4. Check window against user-defined blacklist
	string filename = StringFileName(&filepath, false);
	string class = GetWindowClass(hwnd);
	if (WindowIsBlacklisted(&filename, &class)) {
		Print(L"(blacklisted) ");
		Print(L"add window %s", GetAppHost(hwnd) != hwnd ? L"(uwp) " : L"");
		PrintWindowX(hwnd);
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
		// Since I reset by truncating (setting AppsCount to 0), make sure to overwrite all struct fields when reusing array slots
		app = &Apps[AppsCount++];
		if (app->icon) DestroyIcon(app->icon); // In fact, I have to do a little lazy cleanup
		app->path = filepath;
		app->name = GetAppName(&filepath);
		app->icon = GetAppIcon(&filepath);
		app->windowsCount = 0; // Same truncation trick here (but hwnds are not structs, so no fields to overwrite)
	}
	if (app->windowsCount >= countof(app->windows)) {
		Error(Switcher, L"ERROR reached max. windows for app\n");
		return;
	}
	// 7. Add window to app's window list
	app->windows[app->windowsCount++] = hwnd;
	Print(L"add window %s", GetAppHost(hwnd) != hwnd ? L"(uwp) " : L"");
	PrintWindowX(hwnd);
}

static void ActivateWindow(handle hwnd)
{
	// Find existing app and window
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
			// Shift other windows down to position of 'window' and put 'window' first
			memmove(&app->windows[1], &app->windows[0], sizeof app->windows[0] * (window - app->windows)); // i.e. index
			app->windows[0] = hwnd;
		}
		if (app > &Apps[0]) { // Don't shift if already first
			// Shift other apps down to position of 'app' and put 'app' first
			struct app temp = *app; // This is a 1.6kb copy
			memmove(&Apps[1], &Apps[0], sizeof Apps[0] * (app - Apps)); // i.e. index
			Apps[0] = temp;
		}
	}
}

static BOOL CALLBACK _AddToSwitcher(HWND hwnd, LPARAM lparam)
{
	// EnumWindowsProc used by UpdateApps: Call AddToSwitcher with every top-level window enumerated by EnumWindows
	(*((int *)lparam))++; // Increment window counter arg
	AddToSwitcher(hwnd);
	return true; // Return true to continue EnumWindows
}

static void UpdateApps(void)
{
	// Rebuild the whole window list and measure how long it takes
	i64 start = StartMeasuring();
	int windowsCount = 0;
	AppsCount = 0;
	EnumWindows(_AddToSwitcher, (LPARAM)&windowsCount); // _AddToSwitcher calls AddToSwitcher with every top-level window enumerated by EnumWindows
	AddToHistory(GetForegroundWindow()); // "Activate" foreground window to record it in window activation history
	for (int i = HistoryCount-1; i >= 0; i--) {
		ActivateWindow(History[i]);
	}
	Log(L"%i windows, %llims elapsed\n", windowsCount, FinishMeasuring(start));
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
		Error(Switcher, L"invalid state: no SelectedApp or no SelectedWindow");
		Debug(L"invalid state: no SelectedApp or no SelectedWindow");
		return;
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
		PrintWindowX(*SelectedWindow);
	}
}

static void ResizeSwitcher(void)
{
	// Use the monitor where the mouse pointer is currently placed (#5)
	POINT mousePos = { 0 };
	GetCursorPos(&mousePos);
	MONITORINFO mi = { .cbSize = sizeof(MONITORINFO) };
	GetMonitorInfoW(MonitorFromPoint(mousePos, MONITOR_DEFAULTTONEAREST), &mi);

	u32   iconsWidth = AppsCount * Config.iconWidth;
	u32 paddingWidth = AppsCount * Config.iconHorzPadding * 2;
	u32  marginWidth =         2 * Config.switcherHorzMargin;

	u32 w = iconsWidth + paddingWidth + marginWidth;
	u32 h = Config.switcherHeight;
	i32 x = mi.rcMonitor.left + (mi.rcMonitor.right - mi.rcMonitor.left - w) / 2;
	i32 y = mi.rcMonitor.top + (mi.rcMonitor.bottom - mi.rcMonitor.top - h) / 2;

	MoveWindow(Switcher, x, y, w, h, false); // Yes, "MoveWindow" means "ResizeWindow"

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

static void ShowSwitcher(void)
{
	//ReceiveLastInputEvent();
	ShowWindow(Switcher, SW_SHOWNA);
	if (SetForegroundWindow(Switcher)) {
		LockSetForegroundWindow(LSFW_LOCK);
	}
}

static void CancelSwitcher(bool restore)
{
	Print(L"cancel/close\n");
	HideWindow(Switcher);
	LockSetForegroundWindow(LSFW_UNLOCK);
	if (restore) {
		// TODO Check that the restore window is not the second most recently
		//      activated window (after my own switcher window), so that I
		//      don't needlessly switch to the window that will naturally
		//      become activated once my switcher window hides
		ShowWindowX(RestorableWindow);
	}
	// Reset selection
	SelectedApp = null;
	SelectedWindow = null;
	RestorableWindow = null;
}

//static void CloseWindows(handle hwnd) // Alternative to TerminateWindowProcess
//{
//	if (AppsCount <= 0) {
//		return;
//	}
//	struct app *app = null;
//	handle *window = null;
//	for (int i = 0; i < AppsCount; i++) {
//		app = &Apps[i];
//		for (int j = 0; j < app->windowsCount; j++) {
//			if (app->windows[j] == hwnd) {
//				Print(L"quit app %s\n", app->name);
//				for (int k = 0; k < app->windowsCount; k++) {
//					CloseWindowX(app->windows[k]);
//				}
//				return;
//			}
//		}
//	}
//}

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

static LRESULT CALLBACK KeyboardHookProcedure(int code, WPARAM wparam, LPARAM lparam)
{
	if (code < 0) { // "If code is less than zero, the hook procedure must pass the message to..." blah blah blah read the docs
		return CallNextHookEx(null, code, wparam, lparam);
	}

	KBDLLHOOKSTRUCT *kbd = (KBDLLHOOKSTRUCT *)lparam;
	u32 keyCode = kbd->vkCode;
	bool keyDown = !(bool)(kbd->flags & LLKHF_UP);
	
	#ifdef _DEBUG
	if ((kbd->flags & LLKHF_INJECTED) == LLKHF_INJECTED) {
		u16 keyName[32] = {0};
		int length = GetKeyNameTextW(kbd->scanCode << 16, keyName, countof(keyName));
		Log(L"skip INJECTED INPUT %s %s\n", keyCode ? keyName : L"", keyCode ? keyDown ? L"down" : L"up" : L"");
		goto passMessage;
	}
	#endif

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
			UpdateApps();
			if (AppsCount <= 0) {
				Print(L"no apps\n");
				goto passMessage;
			}
			ReceiveLastInputEvent();
			SelectFirstWindow(); // Initialize selection
			if (*SelectedWindow == GetCoreWindow(GetForegroundWindow())) { // Don't select next when on blank desktop
				SelectNextWindow(true, shiftHeld, !Config.wrapbump || !keyRepeat);
			}
			if (Config.showSwitcherForApps) {
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before ShowSwitcher
				RedrawSwitcher();
				ShowSwitcher();
			}
			if (Config.fastSwitchingForApps) {
				ShowWindowX(*SelectedWindow);
			}
			// Since I "own" this hotkey system-wide, I consume this message unconditionally
			// This will consume the key1Down event. Windows will not know that key1 was pressed
			goto consumeMessage;
		}
		// First Alt-Tilde/Backquote
		if (key2Down && mod2Held && Config.switchWindows) {
			UpdateApps();
			if (AppsCount <= 0) {
				Print(L"no apps\n");
				goto passMessage;
			}
			ReceiveLastInputEvent();
			SelectFirstWindow(); // Initialize selection
			SelectNextWindow(false, shiftHeld, !Config.wrapbump || !keyRepeat);
			if (Config.showSwitcherForWindows) {
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before ShowSwitcher
				RedrawSwitcher();
				ShowSwitcher();
			}
			if (Config.fastSwitchingForWindows) {
				ShowWindowX(*SelectedWindow);
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
			ReceiveLastInputEvent();
			ShowWindowX(*SelectedWindow);
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
					ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before ShowSwitcher
					RedrawSwitcher();
					ShowSwitcher();
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
					ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before ShowSwitcher
					RedrawSwitcher();
					ShowSwitcher();
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
				ShowWindowX(*SelectedWindow);
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
				CloseWindowX(Switcher);
				goto consumeMessage;
			}
			// Alt-Q - close all windows of selected application
			if (keyQDown) {
				//CloseWindows(*SelectedWindow);
				TerminateWindowProcess(*SelectedWindow);
				if (SelectedApp == &Apps[AppsCount-1] && AppsCount > 1) {
					SelectedApp--;
				}
				UpdateApps();
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before ShowSwitcher
				RedrawSwitcher();
				goto consumeMessage;
			}
			// Alt-W or Alt-Delete - close selected window
			if (keyWDown || deleteDown) {
				CloseWindowX(*SelectedWindow);
				UpdateApps();
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before ShowSwitcher
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
				//HideWindow(*SelectedWindow); // WARNING Can't use this because we filter out hidden windows
				goto consumeMessage;
			}
			// Alt-B - display app name and window class to user, to add in blacklist
			if (keyBDown) {
				handle selectedWindow = *SelectedWindow; // CancelSwitcher resets SelectedWindow
				CancelSwitcher(false);
				ReportWindowHandle(Switcher, selectedWindow);
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
	// Make the codepath hot to prevent kbd hook from timing out on first use
	UpdateApps();
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
	AddToHistory(hwnd);
}

static i64 WindowProcedure(handle hwnd, u32 message, u64 wparam, i64 lparam)
{
	switch (message) {
		case WM_CREATE:
			OnWindowCreate();
			break;
		case WM_PAINT:
			OnWindowPaint();
			break;
		case WM_ACTIVATEAPP:
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
		default: {
			return DefWindowProcW(hwnd, message, wparam, lparam);
		}
	}
	return 0;
}

static VOID CALLBACK EventHookProcedure(HWINEVENTHOOK hook, ULONG event, HWND hwnd, LONG objectid, LONG childid, ULONG threadid, ULONG timestamp)
{
	switch (event) {
		case EVENT_SYSTEM_FOREGROUND:
			Print(L"FOREGROUND WINDOW EVENT\n");
			OnShellWindowActivated(hwnd);
			break;
		case EVENT_OBJECT_UNCLOAKED:
			Print(L"UNCLOAKED WINDOW EVENT\n");
			OnShellWindowActivated(hwnd);
			break;
	}
}

static bool HasAutorunLaunchArgument(u16 *args)
{
	return !wcscmp(args, L"--autorun");
}

static void AskAutorun(void)
{
	#ifndef _DEBUG // Can't be bothered to be asked about this every single debug run
	SetAutorun(Ask(Switcher, L"Start cmdtab automatically?\nRelaunch cmdtab.exe to change your mind."), L"cmdtab", L"--autorun");
	#endif
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

static int RunCmdTab(handle instance, u16 *args)
{
	handle mutex;
	//handle mainWindow; // Switcher
	handle keyboardHook, eventHook1, eventHook2;

	if (!HasAutorunLaunchArgument(args)) {
		AskAutorun();
	}
	if (AlreadyRunning(&mutex)) {
		QuitSecondInstance();
	}
	
	//
	i32 _ = CoInitialize(null);

	// Initialize runtime-dependent settings, for example dependent on user's current kbd layout
	InitConfig();

	// Create switcher window
	WNDCLASSEXW wcex = {0};
	wcex.cbSize = sizeof wcex;
	wcex.lpfnWndProc = (WNDPROC)WindowProcedure;
	wcex.hInstance = instance;
	wcex.hCursor = LoadCursor(instance, IDC_ARROW);
	wcex.lpszClassName = L"cmdtabSwitcher";
	Switcher = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, MAKEINTATOM(RegisterClassExW(&wcex)), null, 0, 0, 0, 0, 0, null, null, instance, null);
	// Clear all window styles for very plain window
	SetWindowLongW(Switcher, GWL_STYLE, 0);
	// Rounded window corners
	DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
	DwmSetWindowAttribute(Switcher, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners);

	// Install keyboard hook and event hooks for foreground window changes. Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20130930-00/?p=3083
	keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, (HOOKPROC)KeyboardHookProcedure, null, 0);
	eventHook1 = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, null, EventHookProcedure, 0, 0, WINEVENT_OUTOFCONTEXT);
	eventHook2 = SetWinEventHook(EVENT_OBJECT_UNCLOAKED, EVENT_OBJECT_UNCLOAKED, null, EventHookProcedure, 0, 0, WINEVENT_OUTOFCONTEXT);
	
	// Run message loop
	for (MSG msg; GetMessageW(&msg, Switcher, 0, 0) > 0;) DispatchMessageW(&msg); // Not handling -1 errors. Whatever
	
	// Uninstall keyboard & event hooks
	UnhookWindowsHookEx(keyboardHook);
	UnhookWinEvent(eventHook1);
	UnhookWinEvent(eventHook2);

	CoUninitialize();

	if (mutex) {
		ReleaseMutex(mutex);
		//CloseHandle(hMutex);
	}
	Print(L"cmdtab quit\n");
	bool hasLeaks = _CrtDumpMemoryLeaks();
	return hasLeaks;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	//#ifdef _DEBUG
	//FILE *conout = null;
	//AllocConsole();
	//freopen_s(&conout, "CONOUT$", "w", stdout);
	//#endif
	return RunCmdTab(hInstance, lpCmdLine);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return RunCmdTab(hInstance, GetCommandLineW());
}