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
#include <crtdbg.h>
#include <shellapi.h> // Only used by StringFileName for PathFindFileNameW?
#include <shlwapi.h>
#include <dwmapi.h>
#include <pathcch.h>
#include <strsafe.h>
#include <initguid.h>
#include <commoncontrols.h>
#include <mmsystem.h> // PlaySound

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shlwapi.lib") // Only used by StringFileName for PathFindFileNameW?
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "pathcch.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "winmm.lib") // PlaySound

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

static string UWPAppHostExe      = string(L"ApplicationFrameHost.exe");
static string UWPAppHostClass    = string(L"ApplicationFrameWindow");
static string UWPCoreWindowClass = string(L"Windows.UI.Core.CoreWindow");
static string UWPAppsPath        = string(L"C:\\Program Files\\WindowsApps");

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

static void Print(u16 *fmt, ...)
{
	#ifdef _DEBUG
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	#ifdef _MSC_VER
	  OutputDebugStringW(buffer);
	#else
	  fputws(buffer, stdout);
	#endif
	#endif
}

static void Log(u16 *fmt, ...)
{
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	#ifdef _MSC_VER
	  OutputDebugStringW(buffer);
	#else
	  fputws(buffer, stdout);
	#endif
}

static void Error(handle hwnd, u16 *fmt, ...)
{
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	#ifdef _MSC_VER
	  OutputDebugStringW(buffer);
	#else
	  fputws(buffer, stdout);
	#endif
	SetForegroundWindow(GetLastActivePopup(hwnd));
	MessageBoxW(hwnd, buffer, L"cmdtab", MB_OK | MB_ICONERROR | MB_TASKMODAL);
}

static bool Ask(handle hwnd, u16 *fmt, ...)
{
	u16 buffer[2048];
	va_list args;
	va_start(args, fmt);
	StringCchVPrintfW(buffer, countof(buffer)-1, fmt, args);
	va_end(args);
	SetForegroundWindow(GetLastActivePopup(hwnd));
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
	if (path.ok) {
		path.length = length;
	} else {
		Print(L"WARNING couldn't get exe path for process with pid: %u\n", pid);
	}
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
	if (hwnd) {
		#ifdef _DEBUG
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
		#else
		Print(L"%p\n", hwnd);
		#endif
	} else {
		Print(L"null hwnd\n");
	}
}

static string GetAppName(string *filepath)
{
	// Query version info for pretty application name. See cmdtab's own res/cmdtab.rc file for an idea of the data that is queried

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
		Log(L"suspicious SHGetFileInfoW return"); // SHGetFileInfoW has special return with SHGFI_SYSICONINDEX flag, which I think should never fail (docs unclear?)
		__debugbreak();
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
	if (!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_DWORD, null, &value, &size)) {
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
	(void)success;
}

static bool IsKeyDown(u32 key)
{
	return (bool)(GetAsyncKeyState(key) & 0x8000);
}

static bool ReportWindowHandle(handle hwnd, handle target)
{
	// Tell user the executable name and class so they can add it to the blacklist
	string class = GetWindowClass(target);
	string path = GetExePath(target);
	string name = StringFileName(&path, false);

	u16 buffer[2048];
	StringCchPrintfW(buffer, countof(buffer)-1, L"Add to blacklist?\n\nName:\t%s\nClass:\t%s\n\nPress Ctrl+C to copy this message.\n\nNote: Blacklist not working yet, it does nothing, TODO!", name.text, class.text);
	SetForegroundWindow(GetLastActivePopup(hwnd));
	return MessageBoxW(hwnd, buffer, L"cmdtab", MB_OK | MB_ICONASTERISK | MB_TASKMODAL) == IDYES;
}

static void ShowWindowX(handle hwnd) // ShowWindow already taken
{
	if (GetForegroundWindow() != hwnd) {
		if (IsIconic(hwnd)) {
			ShowWindow(hwnd, SW_RESTORE);
		} else {
			ShowWindow(hwnd, SW_SHOWNA);
		}
		if (SetForegroundWindow(GetLastActivePopup(GetAppHost(hwnd)))) {
			Print(L"switch to ");
			PrintWindowX(hwnd);
		} else {
			Print(L"ERROR couldn't switch to:");
			PrintWindowX(hwnd);
		    PlaySound(L"SystemHand", NULL, SND_ASYNC);
			//CancelSwitcher(); // need forward decl
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

struct ini {
	// Hotkeys
	struct { u32 mod, key, enabled; } hotkeyForApps;
	struct { u32 mod, key, enabled; } hotkeyForWindows;
	// Behavior
	bool askAutorun;
	bool groupByApp; // TODO Can not disable yet
	bool fastSwitchingForApps;
	bool fastSwitchingForWindows;
	bool showSwitcherForApps;
	bool showSwitcherForWindows;
	bool wrapbump;
	// Appearance
	bool darkmode;
	u32 switcherHeight;
	u32 switcherHorzMargin;
	u32 switcherVertMargin;
	u32 iconWidth;
	u32 iconHorzPadding;
	// Blacklist
	struct identifier {
		u16 *filename;          // Filename without file extension, ex. "explorer". Can be null
		u16 *windowClass;       // Window class name. Can be null
	} blacklist[32];
};

struct app {
	string path;                // Full path of app executable
	string name;                // Product description/name, or filename without extension as fallback
	handle icon;                // Big app icon
	handle windows[64];         // App windows that can be switched to
	size windowsCount;          // Number of elements in 'windows' array
};

static handle      Mutex;           // Singleton mutex to prevent running more than one cmdtab instance
static struct ini  Config;          // cmdtab settings
static handle      HistoryHooks[2]; // Hook handles for WinEvent window activation hooks
static handle      History[128];    // Window handles from window activation events (MRU). Deduplicated, so prior activations are moved to the front
static size        HistoryCount;    // Number of elements in 'History' array
static handle      KeyboardHook;    // Handle for low-level keyboard hook
static u16         Keyboard[16];    // 256 bits to track key repeat for low-level keyboard hook
static struct app  Apps[128];       // Apps to be displayed in switcher
static size        AppsCount;       // Number of elements in 'Apps' array
static struct app *SelectedApp;     // Pointer to one of the elements in 'Apps' array. The app for the 'SelectedWindow'
static handle     *SelectedWindow;  // Currently selected window in switcher. Non-NULL indicates switcher is active
// GUI
static handle      Switcher;        // Handle for main window (aka. switcher)
static handle      DrawingContext;  // Drawing context for double-buffered drawing of main window
static handle      DrawingBitmap;   // Off-screen bitmap used for double-buffered drawing of main window
static RECT        DrawingRect;     // Size of the off-screen bitmap
static i32         MouseX, MouseY;  // Mouse position, for highlighting and clicking app icons in switcher

//================
// Initialization
//================

static LRESULT CALLBACK SwitcherWindowProcedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
static VOID    CALLBACK EventHookProcedure(HWINEVENTHOOK hook, ULONG event, HWND hwnd, LONG objectid, LONG childid, ULONG threadid, ULONG timestamp);
static LRESULT CALLBACK KeyboardHookProcedure(INT code, WPARAM wparam, LPARAM lparam);

static void InitConfig(void)
{
	// Default/static cmdtab settings
	Config = (struct ini){
		// Hotkeys
		.hotkeyForApps =    { 0x38, 0x0F, true }, // Alt-Tab in scancodes, must be updated during runtime, see InitConfig
		.hotkeyForWindows = { 0x38, 0x29, true }, // Alt-Tilde/Backquote in scancodes, must be updated during runtime, see InitConfig
		// Behavior
		.askAutorun              = true,
		.groupByApp				 = true,
		.fastSwitchingForApps    = false,
		.fastSwitchingForWindows = true,
		.showSwitcherForApps     = true,
		.showSwitcherForWindows  = false,
		.wrapbump                = true,
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

static bool HasDebugLaunchArgument(u16 *args)
{
	return !!wcsstr(args, L"--debug");
}

static bool HasAutorunLaunchArgument(u16 *args)
{
	return !!wcsstr(args, L"--autorun");
}

static void AskAutorun(void)
{
	#ifndef _DEBUG // Can't be bothered to be asked about this every single debug run
	SetAutorun(Ask(Switcher, L"Start cmdtab automatically?\nRelaunch cmdtab.exe to change your mind."), L"cmdtab", L"--autorun");
	#endif
}

static bool AlreadyRunning(void)
{
	Mutex = CreateMutexW(null, true, L"cmdtabMutex");
	return GetLastError() == ERROR_ALREADY_EXISTS;
}

static void QuitSecondInstance(void)
{
	MessageBoxW(null, L"cmdtab is already running.", L"cmdtab", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
	ExitProcess(0);
}

static void InitWindowActivationTracking(void)
{
	// Install event hooks for foreground window changes. Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20130930-00/?p=3083
	HistoryHooks[0] = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, null, EventHookProcedure, 0, 0, WINEVENT_OUTOFCONTEXT);
	HistoryHooks[1] = SetWinEventHook(EVENT_OBJECT_UNCLOAKED, EVENT_OBJECT_UNCLOAKED, null, EventHookProcedure, 0, 0, WINEVENT_OUTOFCONTEXT);
}

static void InitKeyboardHook(void)
{
	// Install keyboard hook
	KeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProcedure, null, 0);
}

static void InitSwitcherWindow(handle instance)
{
	// Create switcher window
	WNDCLASSEXW wcex = {0};
	wcex.cbSize = sizeof wcex;
	wcex.lpfnWndProc = SwitcherWindowProcedure;
	wcex.hInstance = instance;
	wcex.hCursor = LoadCursorW(null, IDC_ARROW);
	wcex.lpszClassName = L"cmdtabSwitcher";
	// TOOLWINDOW so the switcher itself does not appear as an Alt-Tab window (see IsAltTabWindow)
	Switcher = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, MAKEINTATOM(RegisterClassExW(&wcex)), null, 0, 0, 0, 0, 0, null, null, instance, null);
	// Clear all window styles for very plain window
	SetWindowLongW(Switcher, GWL_STYLE, 0);
	// Rounded window corners
	DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
	DwmSetWindowAttribute(Switcher, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners);
}

static int RunCmdTab(handle instance, u16 *args)
{
	// Initialize runtime-dependent settings, for example dependent on user's current kbd layout
	InitConfig();

	if (!HasAutorunLaunchArgument(args) && Config.askAutorun) {
		AskAutorun();
	}
	if (AlreadyRunning()) {
		QuitSecondInstance();
	}

	//
	i32 _ = CoInitialize(null);

	InitSwitcherWindow(instance);
	InitWindowActivationTracking();
	InitKeyboardHook();

	// Run message loop
	for (MSG msg; GetMessageW(&msg, Switcher, 0, 0) > 0;) DispatchMessageW(&msg); // Not handling -1 errors. Whatever

	// Uninstall keyboard & event hooks
	UnhookWindowsHookEx(KeyboardHook);
	UnhookWinEvent(HistoryHooks[1]);
	UnhookWinEvent(HistoryHooks[0]);

	//
	CoUninitialize();

	if (Mutex) ReleaseMutex(Mutex);

	Print(L"cmdtab quit\n");
	bool hasLeaks = _CrtDumpMemoryLeaks();
	Print(L"leaks? %s\n", hasLeaks ? L"YES" : L"No leaks.");
	return hasLeaks;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	return RunCmdTab(hInstance, lpCmdLine);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return RunCmdTab(hInstance, GetCommandLineW());
}

//=============
// Application
//=============

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
	// Already added and first?
	if (History[0] == hwnd) {
		Print(L"(already first in history) ");
		Print(L"activate window %s", GetAppHost(hwnd) != hwnd ? L"(uwp) " : L"");
		PrintWindowX(hwnd);
		return;
	}
	// Find 'hwnd' in previous history, shift other hwnds down to position of 'hwnd', and put 'hwnd' first
	for (int i = 1; i < HistoryCount; i++) {
		if (History[i] == hwnd) {
			memmove(&History[1], &History[0], sizeof History[0] * i);
			History[0] = hwnd;
			return;
		}
	}

	// 'hwnd' is not already tracked in activation history; so validate and add it

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
		return; // Don't have to print error, since GetExePath already does that
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

	// 'hwnd' is valid and should be added to activation history

	// Shift all hwnds down one position and put 'hwnd' first
	if (HistoryCount < countof(History)) {
		HistoryCount++;
	}
	memmove(&History[1], &History[0], sizeof History[0] * HistoryCount);
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
		// These prints are very spammy since looots of windows are ignored
		//Print(L"(ignored) ");
		//Print(L"add window %s", GetAppHost(hwnd) != hwnd ? L"(uwp) " : L"");
		//PrintWindowX(hwnd);
		return;
	}
	// 3. Get module filepath, abort if failed (most likely access denied)
	string filepath = GetExePath(hwnd);
	if (!filepath.ok) {
		return; // Don't have to print error, since GetExePath already does that
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
			memmove(&app->windows[1], &app->windows[0], sizeof app->windows[0] * (window - app->windows)); // i.e. index of 'window'
			app->windows[0] = hwnd;
		}
		if (app > &Apps[0]) { // Don't shift if already first
			// Shift other apps down to position of 'app' and put 'app' first
			struct app temp = *app; // This is a 1.6kb copy
			memmove(&Apps[1], &Apps[0], sizeof Apps[0] * (app - Apps)); // i.e. index of 'app'
			Apps[0] = temp;
		}
	}
}

static BOOL CALLBACK _AddToSwitcher(HWND hwnd, LPARAM lparam)
{
	// EnumWindowsProc used by UpdateApps to call AddToSwitcher with every top-level window enumerated by EnumWindows
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
	// Add all running windows on the system to the Apps array, filtering unwanted
	// windows and grouping them by app
	// EnumWindows enumerates windows by Z-order, not activation order
	EnumWindows(_AddToSwitcher, (LPARAM)&windowsCount); // _AddToSwitcher calls AddToSwitcher with every top-level window enumerated by EnumWindows
	// Now run all added windows through our own History record, sorting them
	// by the order we've observed them to be activated
	// First time this function is called we may not have observed any window
	// activations yet, but we know that the foreground window is the most
	// recently activated window
	AddToHistory(GetForegroundWindow()); // "Activate" foreground window to record it in window activation history
	for (int i = HistoryCount-1; i >= 0; i--) {
		ActivateWindow(History[i]);
	}
	Log(L"%i windows, %llims elapsed\n", windowsCount, FinishMeasuring(start));
	PrintApps();
}

static void SelectFirstApp(void)
{
	// Contrary to 0da13fd's commit message, this function is not split into
	// *App and *Window variants like the SelectNext* functions below are
	/*dbg*/ handle *oldWindow = SelectedWindow;
	if (AppsCount > 0) {
		SelectedApp = &Apps[0];
		SelectedWindow = &Apps[0].windows[0];
	} else {
		SelectedApp = null;
		SelectedWindow = null;
		Print(L"nothing to select\n");
	}
	/*dbg*/
	if (oldWindow != SelectedWindow) {
		Print(L"select first ");
		PrintWindowX(*SelectedWindow);
	}
}

static void SelectNextApp(bool reverse, bool wrap)
{
	//assert(SelectedApp && SelectedWindow);
	/*dbg*/ handle *oldWindow = SelectedWindow;
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
	/*dbg*/
	if (oldWindow != SelectedWindow) {
		Print(L"select app ");
		PrintWindowX(*SelectedWindow);
	}
}

static void SelectNextWindow(bool reverse, bool wrap)
{
	//assert(SelectedApp && SelectedWindow);
	/*dbg*/ handle *oldWindow = SelectedWindow;
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
	/*dbg*/
	if (oldWindow != SelectedWindow) {
		Print(L"select window ");
		PrintWindowX(*SelectedWindow);
	}
}

static void ResizeSwitcher(void)
{
	// Use the monitor where the mouse pointer is currently placed (#5)
	POINT mousePos = {0};
	MONITORINFO mi = {.cbSize = sizeof mi};
	GetCursorPos(&mousePos);
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

	u32 ICON_WIDTH = Config.iconWidth;
	u32 ICON_PAD = Config.iconHorzPadding;
	u32 HORZ_PAD = Config.switcherHorzMargin;
	u32 VERT_PAD = Config.switcherVertMargin;

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

	// Window rect
	RECT windowRect = {0};
	GetClientRect(Switcher, &windowRect);

	// Invalidate & draw window background
	RedrawWindow(Switcher, null, null, RDW_INVALIDATE | RDW_ERASE);
	FillRect(DrawingContext, &windowRect, windowBackground);

	// Select pen & brush for RoundRect (used to draw selection rectangle)
	HPEN oldPen = (HPEN)SelectObject(DrawingContext, selectionOutline);
	HBRUSH oldBrush = (HBRUSH)SelectObject(DrawingContext, selectionBackground);

	// Select text font, color & background for DrawTextW (used to draw title text)
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

		if (app != SelectedApp) {
			// Draw only icon, with window background
			DrawIconEx(DrawingContext, left, top, app->icon, width, height, 0, windowBackground, DI_NORMAL);
		} else {
			// Draw selection rectangle and icon, with selection background
			RoundRect(DrawingContext, left - SEL_VERT_OFF, top - SEL_HORZ_OFF, right + SEL_VERT_OFF, bottom + SEL_HORZ_OFF, SEL_RADIUS, SEL_RADIUS);
			DrawIconEx(DrawingContext, left, top, app->icon, width, height, 0, selectionBackground, DI_NORMAL);

			// Draw app name
			u16 *titleText = app->name.text;
			RECT titleRect = {0}; // DrawTextW uses a destination rect for drawing

			// Measure text width
			DrawTextW(DrawingContext, titleText, -1, &titleRect, DT_CALCRECT | DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
			i32 titleWidth = (titleRect.right - titleRect.left) + 2; // Hmm, measured size seems a teensy bit too small, so +2

			titleRect.left = left;
			titleRect.right = right;
			titleRect.bottom = windowRect.bottom;

			i32 widthDiff = titleWidth - (titleRect.right - titleRect.left);
			if (widthDiff > 0) {
				// titleRect is too small for text, grow the rect
				titleRect.left -= (i32)ceil(widthDiff / 2.0);
				titleRect.right += (i32)floor(widthDiff / 2.0);
			}

			titleRect.bottom -= ICON_PAD; // *** this lifts the app text up a little ***

			// If the app name, when centered, extends past window bounds, adjust label position to be inside window bounds
			if (titleRect.right > windowRect.right) {
				titleRect.left -= (titleRect.right - windowRect.right) + ICON_PAD;
				titleRect.right = windowRect.right - ICON_PAD;
			}
			if (titleRect.left < 0) {
				titleRect.right += ICON_PAD + abs(titleRect.left);
				titleRect.left   = ICON_PAD + 0;
			}

			///* dbg */ FillRect(DrawingContext, &titleRect, CreateSolidBrush(RGB(255, 0, 0))); // Check the size of the text box
			DrawTextW(DrawingContext, titleText, -1, &titleRect, DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
		}
	}
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
	SendInput(1, &(INPUT){.type = INPUT_MOUSE}, sizeof(INPUT));
	//SendInput(1, &(INPUT){.type = INPUT_KEYBOARD}, sizeof(INPUT));
}

static void SendModKeysUp(void)
{
	SendInput(2,
		(INPUT[]) {
			(INPUT){.type = INPUT_KEYBOARD, .ki.wVk = Config.hotkeyForApps.mod, .ki.dwFlags = KEYEVENTF_KEYUP},
			(INPUT){.type = INPUT_KEYBOARD, .ki.wVk = Config.hotkeyForWindows.mod, .ki.dwFlags = KEYEVENTF_KEYUP},
		},
		sizeof(INPUT));
}

static void ShowSwitcher(void)
{
	ReceiveLastInputEvent();
	ShowWindowX(Switcher);
	LockSetForegroundWindow(LSFW_LOCK);
}

static void CancelSwitcher(void)
{
	Print(L"cancel/close\n");
	HideWindow(Switcher);
	LockSetForegroundWindow(LSFW_UNLOCK);
	// Reset selection
	SelectedApp = null;
	SelectedWindow = null;
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

	// As a sledgehammer to avoid stuck mod keys, I send mod keyups.
	// This kbd hook is then sent that mod keyup which would close the
	// switcher. But we need to avoid that, so we skip events that are
	// injected mod keyups.
	bool injected = (kbd->flags & LLKHF_INJECTED);
	if (injected && !keyDown && (keyCode == Config.hotkeyForApps.mod || keyCode == Config.hotkeyForWindows.mod)) {
		Print(L"skip INJECTED INPUT %u %s\n", keyCode, keyDown ? L"down" : L"up");
		goto passMessage;
	}

	// (SelectedWindow == NULL) means Switcher is not active/visible
	if (!SelectedWindow) {

		bool keyRepeat = SetKey(keyCode, keyDown); // Manually track key repeat
		bool shiftHeld = IsKeyDown(VK_LSHIFT) || IsKeyDown(VK_RSHIFT) || IsKeyDown(VK_SHIFT);

		// ========================================
		// Activation
		// ========================================

		bool hotkeyForApps =
			keyCode == Config.hotkeyForApps.key && keyDown &&
			IsKeyDown(Config.hotkeyForApps.mod) &&
			Config.hotkeyForApps.enabled;

		// First Alt-Tab
		if (hotkeyForApps) {
			UpdateApps();
			if (AppsCount <= 0) {
				Print(L"no apps\n");
				goto passMessage;
			}
			SelectFirstApp(); // Initialize selection
			if (*SelectedWindow == GetCoreWindow(GetForegroundWindow())) { // Don't select next when on blank desktop
				SelectNextApp(shiftHeld, !Config.wrapbump || !keyRepeat);
			}
			if (Config.showSwitcherForApps) {
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before ShowSwitcher
				RedrawSwitcher();
				ShowSwitcher();
			}
			if (Config.fastSwitchingForApps) {
				ReceiveLastInputEvent();
				ShowWindowX(*SelectedWindow);
			}
			// Since I "own" this hotkey system-wide, I consume this message unconditionally
			// This will consume the key1Down event. Windows will not know that key1 was pressed
			goto consumeMessage;
		}

		bool hotkeyForWindows =
			keyCode == Config.hotkeyForWindows.key && keyDown &&
			IsKeyDown(Config.hotkeyForWindows.mod) &&
			Config.hotkeyForWindows.enabled;

		// First Alt-Tilde/Backquote
		if (hotkeyForWindows) {
			UpdateApps();
			if (AppsCount <= 0) {
				Print(L"no apps\n");
				goto passMessage;
			}
			SelectFirstApp(); // Initialize selection
			SelectNextWindow(shiftHeld, !Config.wrapbump || !keyRepeat);
			if (Config.showSwitcherForWindows) {
				ResizeSwitcher(); // NOTE Must call ResizeSwitcher after UpdateApps, before RedrawSwitcher & before ShowSwitcher
				RedrawSwitcher();
				ShowSwitcher();
			}
			if (Config.fastSwitchingForWindows) {
				ReceiveLastInputEvent();
				ShowWindowX(*SelectedWindow);
			}
			// Since I "own" this hotkey system-wide, I consume this message unconditionally
			// This will consume the key2Down event. Windows will not know that key2 was pressed
			goto consumeMessage;
		}

	} else { // (SelectedWindow != NULL) means switcher is active/visible

		// ========================================
		// Deactivation
		// ========================================

		bool mod1Up = keyCode == Config.hotkeyForApps.mod && !keyDown;
		bool mod2Up = keyCode == Config.hotkeyForWindows.mod && !keyDown;

		if (mod1Up || mod2Up) {
			// Mod keyup - switch to selected window
			// Always pass/never consume mod key up, because:
			// 1) cmdtab never hooks mod keydowns
			// 2) cmdtab hooks mod keyups, but always passes it through on pain
			//    of getting a stuck Alt key because of point 1
			// EDIT:
			// So after much experimentation it seems to work best to consume
			// these mod keyups and manually send mod keyups. I dunno why.
			SendModKeysUp();
			ReceiveLastInputEvent();
			ShowWindowX(*SelectedWindow);
			CancelSwitcher();
			goto consumeMessage;
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
				SelectNextApp(shiftHeld || prevDown, !Config.wrapbump || !keyRepeat);
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
				SelectNextWindow(shiftHeld, !Config.wrapbump || !keyRepeat);
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
				CancelSwitcher();
				// Since our swicher is active, I "own" this hotkey, so I consume this message
				// This will consume the enterDown event. Windows will not know that Enter was pressed
				goto consumeMessage;
			}
			// Alt-Esc - cancel switching and restore initial window
			if (escDown) { // BUG There's a bug here if mod1Held and mod2Held aren't both the same key. In that case I'll still pass through the key event, even though I should own it since it's not Alt-Esc
				CancelSwitcher();
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
					SelectedWindow = &SelectedApp->windows[0];
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
				CancelSwitcher();
				ReportWindowHandle(Switcher, selectedWindow);
				goto consumeMessage;
			}

			// ========================================
			// Debug
			// ========================================

			bool keyF11Down = keyCode == VK_F11 && keyDown && !keyRepeat;
			bool keyF12Down = keyCode == VK_F12 && keyDown && !keyRepeat;

			if (keyF12Down) {
				Log(L"debug hotkey");
				__debugbreak();
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

static i64 OnSwitcherCreate(void)
{
	// Make the codepath hot to prevent kbd hook from timing out on first use
	UpdateApps();
	return 1;
}

static i64 OnSwitcherPaint(void)
{
	// Double buffered drawing, just blit the DrawingContext
	// I draw off-screen in-memory to 'DrawingContext' whenever changes occur,
	// and whenever I get the WM_PAINT message I just blit the
	// off-screen buffer to screen ("screen" means the 'hdc' returned by
	// BeginPaint passed our 'hwnd')
	PAINTSTRUCT ps;
	if (BeginPaint(Switcher, &ps)) {
		if (ps.fErase) {
		}
		BitBlt(ps.hdc, 0, 0, DrawingRect.right - DrawingRect.left, DrawingRect.bottom - DrawingRect.top, DrawingContext, 0, 0, SRCCOPY);
		EndPaint(Switcher, &ps);
	}
	return 1;
}

static i64 OnSwitcherFocusChange(bool focused)
{
	// Cancel on mouse clicking outside window
	if (focused) {
		Print(L"switcher got focus\n");
		return 0;
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
		MouseX = 0;
		MouseY = 0;
		return 1; // Nu-uh!
	}
}

static i64 OnSwitcherMouseMove(int x, int y)
{
	// The switcher window is sent a mouse move event if the mouse happens to be
	// over the window on window show. So this is to prevent accidental mouse
	// selection.
	// This depends on setting MouseX&Y to 0 at init (already happens because
	// static) and when the switcher becomes inactive
	// Atm resetting is done in OnSwitcherFocusChange, but could be done in CloseSwitcher
	bool skip = !MouseX && !MouseY;
	MouseX = x;
	MouseY = y;
	//Print(L"x%i y%i\n", MouseX, MouseY);
	if (skip) {
		return 0;
	}

	// Iteration logic copy/pasted from RedrawSwitcher, so if something changes
	// there update this:
	u32 ICON_WIDTH = Config.iconWidth;
	u32 ICON_PAD = Config.iconHorzPadding;
	u32 HORZ_PAD = Config.switcherHorzMargin;
	u32 VERT_PAD = Config.switcherVertMargin;

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

		// Vertically extend selection area to top and bottom for uSaBiLiTy
		top = 0;
		height = Config.switcherHeight;
		bottom = top + height;

		bool mouseIsOverIcon = PtInRect(&(RECT){left, top, right, bottom}, (POINT){MouseX, MouseY});
		if (mouseIsOverIcon && SelectedApp != app) {
			SelectedApp = app;
			SelectedWindow = &SelectedApp->windows[0];
			/*dbg*/Print(L"select app ");
			/*dbg*/PrintWindowX(*SelectedWindow);
			RedrawSwitcher();
		}
	}
	return 1;
}

static i64 OnSwitcherMouseLeave(void)
{
	OnSwitcherMouseMove(0, 0);
	return 1;
}

static i64 OnSwitcherMouseUp(void)
{
	// Copy/pasted from (mod1up || mod2up) in KeyboardHookProcedure, so if
	// something changes there, update here:
	if (GetForegroundWindow() != *SelectedWindow) {
		//SendModKeysUp();
		ShowWindowX(*SelectedWindow);
	}
	CancelSwitcher();
	return 1;
}

static i64 OnSwitcherClose(void)
{
	CancelSwitcher();
	if (Ask(Switcher, L"Quit cmdtab?")) {
		DestroyWindow(Switcher);
	}
	return 1;
}

static i64 OnShellWindowActivated(handle hwnd)
{
	AddToHistory(hwnd);
	return 1;
}

static LRESULT CALLBACK SwitcherWindowProcedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
		case WM_CREATE:
			return OnSwitcherCreate();
		case WM_PAINT:
			return OnSwitcherPaint();
		case WM_ACTIVATEAPP:
			return OnSwitcherFocusChange(wparam != WA_INACTIVE);
		case WM_MOUSEMOVE:
			TrackMouseEvent(&(TRACKMOUSEEVENT){.cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = hwnd});
			return OnSwitcherMouseMove(LOWORD(lparam), HIWORD(lparam));
		case WM_MOUSELEAVE:
			return OnSwitcherMouseLeave();
		case WM_LBUTTONUP:
			return OnSwitcherMouseUp();
		case WM_CLOSE:
			return OnSwitcherClose();
		default:
			return DefWindowProcW(hwnd, message, wparam, lparam);
	}
}

static VOID CALLBACK EventHookProcedure(HWINEVENTHOOK hook, ULONG event, HWND hwnd, LONG objectid, LONG childid, ULONG threadid, ULONG timestamp)
{
	switch (event) {
		case EVENT_SYSTEM_FOREGROUND:
			Print(L"FOREGROUND WINDOW EVENT\n");
			/* fallthrough */
		case EVENT_OBJECT_UNCLOAKED:
			Print(L"UNCLOAKED WINDOW EVENT\n");
			OnShellWindowActivated(hwnd);
			break;
	}
}
