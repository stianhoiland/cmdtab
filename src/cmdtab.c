#include "cmdtab.h"

typedef struct title title_t;
typedef struct path path_t;
typedef struct linked_window linked_window_t;
typedef struct windows windows_t;
typedef struct selection selection_t;
typedef struct keyboard keyboard_t;
typedef struct style style_t;
typedef struct gdi gdi_t;
typedef struct config config_t;

typedef unsigned long vkcode;

struct title {
	wchar_t text[(MAX_PATH * 2) + 1];
	int length;
	bool ok;
};

struct path {
	wchar_t text[MAX_PATH + 1];
	unsigned long length; // QueryFullProcessImageNameW() wants 'unsigned long'
	bool ok;
};

struct linked_window {
	path_t exe_path;
	path_t exe_name;
	path_t app_name;
	title_t title;
	void *hico;
	void *hwnd;
	linked_window_t *next_sub_window;
	linked_window_t *prev_sub_window;
	linked_window_t *next_top_window;
	linked_window_t *prev_top_window;
};

struct style {
	int gui_height;
	int gui_horz_margin;
	int gui_vert_margin;
	int icon_width;
	int icon_horz_padding;
};

//==============================================================================
// Structs for app state
//==============================================================================

struct windows {
	linked_window_t array[256]; // How many (filtered!) windows do you really need?!
	int num_windows; // # of windows in 'array', i.e. 'count'
	int num_apps; // # of top-level windows
};

struct selection {
	linked_window_t *current;
	linked_window_t *showed;
	void *restore;
};

struct keyboard {
	char keys[32]; // 256 bits for tracking key repeat
};

struct gdi {
	void *hdc;
	void *hbm;
	int width;
	int height;
	RECT rect;
	//HBRUSH bg;
};

struct config {
	vkcode mod1;
	vkcode key1;
	vkcode mod2;
	vkcode key2;
	bool switch_apps;
	bool switch_windows;
	bool fast_switching_apps; // BUG / TODO Not ideal. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
	bool fast_switching_windows; // BUG / TODO Not ideal. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
	bool gui_for_apps;
	bool gui_for_windows;
	int  gui_delay;
	bool wrap_bump;
	bool ignore_minimized; // TODO? Atm, 'ignore_minimized' affects both Alt+` and Alt+Tab (which may be unexpected. Maybe it should only apply to subwindow switching? Etc.) TODO Should this be called 'restore_minimized'?
	//wchar_t blacklist[32][(MAX_PATH * 1) + 1]; // TODO Blacklist isn't implemented yet
	style_t style; // TODO Maybe not put in config_t?
};

//==============================================================================
// Global variables for Windows API handles
//==============================================================================

//static void *hMutexHandle;
//static void *hMainInstance;
static void *hMainWindow;
//static void *hKeyboardHook;

//==============================================================================
// Global variables for app state
//==============================================================================

static windows_t windows; // Filtered windows, as returned by EnumWindows, filtered by filter and added by add

static selection_t selection; // State for currently selected window

static keyboard_t keyboard; // Must manually track key repeat for the low-level keyboard hook

//static style_t style; // TODO Maybe put in config_t?

static gdi_t gdi; // Off-screen bitmap used for double-buffering

static config_t config;

//==============================================================================
// Uncomfortable forward declarations
//==============================================================================

static BOOL CALLBACK EnumProc(HWND, LPARAM); // Used as callback for EnumWindows

static VOID CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD); // Resolve circular dependency between show_gui and TimerProc

//==============================================================================
// Utility functions
//==============================================================================

static void print(wchar_t *fmt, ...) {
	// Low level debug printing w/ the following features:
	// vararg, wide char, formatted, safe (i.e. vswprintf_s)
	// TODO #ifdef this away for release builds?
	wchar_t wcs[4096];
	va_list argp;
	va_start(argp, fmt);
	vswprintf_s(wcs, countof(wcs), fmt, argp); // BUG Not checking return value for error
	va_end(argp);
	OutputDebugStringW(wcs);
}

static void SetForegroundWindow_ALTHACK(void *hwnd) {
	// TODO / BUG / HACK!
	// Oof, the rabbit hole...
	// There are rules for which processes can set the foreground window.
	// See docs. Sending an Alt key event allows us to circumvent limitations on
	// SetForegroundWindow(). I believe (?!) that the real reason we need this
	// hack as per the current implementation, is that we are technically
	// calling cycle() (which calls show(), which calls SetForegroundWindow())
	// *from the low level keyboard hook* and not from our window thread (?),
	// and the low level keyboard hook is not considered a foreground process.
	// I haven't tested this yet, but I believe the more correct implementation
	// would be to SendMessageW() from the LLKBDH to our window and call cycle()
	// et al. from our window's WndProc. I think this more correct
	// implementation is rather trivial to set up, but I can't be bothered right
	// now. So, Alt key hack it is.
	
	///* dbg */ print(L"alt down\n");
	keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | 0, 0);
	SetForegroundWindow(hwnd);
	keybd_event(VK_MENU, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
	///* dbg */ print(L"alt up\n");
}

//==============================================================================
// Here we go!
//==============================================================================

static title_t title(void *hwnd) {
	title_t title = {
		.text = {0},
		.length = {0},
		.ok = {0}
	};
	title.length = GetWindowTextW(hwnd, title.text, countof(title.text));
	title.ok = (title.length > 0);
	return title;
}

static path_t path(void *hwnd) {
	path_t path = {
		.text = {0},
		.length = countof(((path_t *)0)->text), // BUG / TODO Wait, does size need -1 here or below? NOTE Used as buffer size (in) [in characters! (docs)] and path length (out) by QueryFullProcessImageNameW()
		.ok = {0}
	};
	// How to get the file path of the executable for the process that spawned the window:
	// 1. GetWindowThreadProcessId() to get process id for window
	// 2. OpenProcess() for access to remote process memory
	// 3. QueryFullProcessImageNameW() [Windows Vista] to get full "executable image" name (i.e. exe path) for process
	unsigned long pid;
	GetWindowThreadProcessId(hwnd, &pid);
	void *process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, pid); // NOTE See GetProcessHandleFromHwnd()
	if (!process) {
		print(L"ERROR couldn't open process for window: %s %p\n", title(hwnd).text, hwnd);
		return path;
	}
	bool success = QueryFullProcessImageNameW(process, 0, path.text, &path.length); // NOTE Can use GetLongPathName() if getting shortened paths
	CloseHandle(process);
	if (!success) {
		print(L"ERROR couldn't get exe path for window: %s %p\n", title(hwnd).text, hwnd);
		return path;
	}
	path.ok = success;
	return path;
}

static bool patheq(path_t *path1, path_t *path2) {
	// Compare paths from last to first char since unique paths nevertheless often have long identical prefixes
	if (path1->length != path2->length) {
		return false;
	}
	for (int i = path1->length - 1; i >= 0; i--) {
		if (path1->text[i] != path2->text[i]) {
			return false;
		}
	}
	return true;
}

static path_t filename(wchar_t *path, bool ext) {
	path_t name = {
		.text = {0},
		.length = countof(((path_t *)0)->text), // BUG / TODO Wait, does size need -1 here?
		.ok = {0}
	};
	errno_t cpy_error = wcscpy_s(name.text, name.length, PathFindFileNameW(path)); // BUG Not checking 'cpy_error'
	if (!ext) {
		PathRemoveExtensionW(name.text); // BUG Deprecated, use PathCchRemoveExtension()
	}
	name.length = wcslen(name.text);
	name.ok = (cpy_error == 0);
	return name;
}

static path_t appname(wchar_t *path) {
	// Get pretty application name
	//
	// Don't even ask me bro why it has to be like this
	// Thanks to 'Aric Stewart' for an ok-ish looking version of this that I
	// adapted a little

	// Our return value:
	path_t name = {
		.text = {0},
		.length = countof(((path_t *)0)->text),
		.ok = {0}
	};

	unsigned length;
	unsigned long size, handle;
	void *version = NULL;
	wchar_t buffer[MAX_PATH];
	wchar_t *result;
	struct {
		unsigned short wLanguage;
		unsigned short wCodePage;
	} *translate;

	size = GetFileVersionInfoSizeW(path, &handle);
	if (!size) {
		goto done;
	}
	version = malloc(size);
	if (!version) {
		goto done;
	}
	if (!GetFileVersionInfoW(path, handle, size, version)) {
		goto done;
	}
	if (!VerQueryValueW(version, L"\\VarFileInfo\\Translation", (void *)&translate, &length)) {
		goto done;
	}

	wsprintfW(buffer, L"\\StringFileInfo\\%04x%04x\\FileDescription", translate[0].wLanguage, translate[0].wCodePage);
	
	if (!VerQueryValueW(version, buffer, (void *)&result, &length)) {
		goto done;
	}
	if (!length || !result) {
		goto done;
	}

	if (length == 1) {
		// Fall back to 'ProductName' if 'FileDescription' is empty
		// Example: Github Desktop
		wsprintfW(buffer, L"\\StringFileInfo\\%04x%04x\\ProductName", translate[0].wLanguage, translate[0].wCodePage);
		
		if (!VerQueryValueW(version, buffer, (void *)&result, &length)) {
			goto done;
		}
		if (!length || !result) {
			goto done;
		}
	}

	name.length = length;
	name.ok = wcscpy_s(name.text, name.length, result) == 0;

	done: {
		free(version);
	}

	return name;
}

static void *icon(wchar_t *path, void *hwnd) {
	//
	// HICON GetHighResolutionIcon(wchar_t *path, void *hwnd);
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
	//
	if (FAILED(CoInitialize(NULL))) { // In case we use COM
		return NULL;
	}
	SHFILEINFOW sfi = {0};
	if (!SHGetFileInfoW(path, 0, &sfi, sizeof sfi, SHGFI_SYSICONINDEX)) { // 2nd arg: -1, 0 or FILE_ATTRIBUTE_NORMAL
		debug(); // SHGetFileInfoW() has special return with SHGFI_SYSICONINDEX flag, which I think should never fail (docs unclear?)
		return NULL;
	}
	IImageList *piml = {0};
	if (FAILED(SHGetImageList(SHIL_JUMBO, &IID_IImageList, (void **)&piml))) {
		return NULL;
	}
	HICON hico = {0};
	int w, h;
	IImageList_GetIcon(piml, sfi.iIcon, ILD_TRANSPARENT, &hico); //ILD_IMAGE
	IImageList_GetIconSize(piml, &w, &h);
	IImageList_Release(piml);
	return hico;
}

static void close(void *hwnd) {
	SendMessageW(hwnd, WM_CLOSE, 0, 0);
}

static void minimize(void *hwnd) {
	SendMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}


static void defaults(config_t *config) {
	// The default key2 is scan code 0x29 (hex) / 41 (decimal)
	// This is the key that is physically below Esc, left of 1 and above Tab
	*config = (config_t){
		.mod1 = VK_LMENU,
		.key1 = VK_TAB,
		.mod2 = VK_LMENU,
		.key2 = MapVirtualKeyW(0x29, MAPVK_VSC_TO_VK_EX),
		.switch_apps = true,
		.switch_windows = true,
		.fast_switching_apps = false, // BUG / TODO Not ideal. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
		.fast_switching_windows = true, // BUG / TODO Not ideal. Have to find a way to show windows without changing their Z-Order (i.e. "previewing" them)
		.gui_for_apps = true,
		.gui_for_windows = false,
		.gui_delay = 0,
		.wrap_bump = true,
		.ignore_minimized = false, // NOTE Currently only observed for subwindow switching and BUG! Move ignore_minimized handling from show() -> selectnext()
		/*
		.blacklist = {
			// TODO Blacklist isn't implemented yet
			L"TextInputHost.exe",
			L"SystemSettings.exe",
			L"ApplicationFrameHost.exe", // BUG Oof, this one is too heavy-handed. This excludes Calculator, for example. Have to find a better way to deal with UWP windows
		},
		*/
		.style = (style_t){
			.gui_height = 128,
			.icon_width = 64,
			.icon_horz_padding = 8,
			.gui_horz_margin = 24,
			.gui_vert_margin = 32,
		},
	};
}


static void show_hwnd(void *hwnd, bool restore) {
	if (restore && IsIconic(hwnd)) {
		ShowWindow(hwnd, SW_RESTORE);
	} else {
		ShowWindow(hwnd, SW_SHOW);
	}
	SetForegroundWindow_ALTHACK(hwnd);
}

static void size_gui(void *hwnd, style_t *style, int num_apps) {
	int    icon_width = num_apps * style->icon_width;
	int padding_width = num_apps * style->icon_horz_padding * 2;
	int  margin_width =        2 * style->gui_horz_margin;

	int w = icon_width + padding_width + margin_width;
	int h = style->gui_height;
	int x = GetSystemMetrics(SM_CXSCREEN) / 2 - (w / 2);
	int y = GetSystemMetrics(SM_CYSCREEN) / 2 - (h / 2);
	
	MoveWindow(hwnd, x, y, w, h, false);
}

static void show_gui(void *hwnd, unsigned delay) {
	KillTimer(hwnd, 1);
	if (delay > 0) {
		SetTimer(hwnd, 1, delay, TimerProc);
		return;
	}
	show_hwnd(hwnd, true);
}

static VOID TimerProc(HWND hWnd, UINT uMessage, UINT_PTR uEventId, DWORD dwTime) {
	show_gui(hWnd, 0);
}


static bool filter(void *hwnd) {
	if (config.ignore_minimized && IsIconic(hwnd)) {
		return false;
	}

	// Raymond Chen: https://devblogs.microsoft.com/oldnewthing/20200302-00/?p=103507
	if (IsWindowVisible(hwnd) == false) {
		return false;
	}
	int cloaked = 0;
	int success = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked); // Windows 8
	if (success && cloaked) {
		return false;
	}

	// "A tool window [i.e. WS_EX_TOOLWINDOW] does not appear in the taskbar or
	// in the dialog that appears when the user presses ALT+TAB." (docs)
	if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) {
		return false;
	}

	// Here starts the witch hunt of eliminating unsightly windows through
	// arcane spells and incantations 

	path_t exe_path = path(hwnd);
	path_t exe_name = filename(exe_path.text, false);
	if (!exe_path.ok || !exe_name.ok) {
		print(L"whoops, bad exe path?! %p\n", hwnd);
		return false;
	}

	wchar_t class_name[MAX_PATH] = {0};
	GetClassNameW(hwnd, class_name, countof(class_name));

	#define wcseq(str1, str2) wcscmp(str1, str2) == 0

	if (wcseq(class_name, L"Windows.UI.Core.CoreWindow") ||
		wcseq(class_name, L"ApplicationFrameWindow") && wcseq(exe_name.text, L"explorer") ||
		wcseq(class_name, L"Xaml_WindowedPopupClass")) {
		return false;
	}

	/*
	if (!(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_OVERLAPPEDWINDOW)) {
		return false;
	}
	*/

	return true;
}

static void add(windows_t *windows, void *hwnd) {
	path_t exe_path = path(hwnd);

	if (!exe_path.ok) {
		print(L"whoops, bad exe path?! %p\n", hwnd);
		return;
	}

	path_t exe_name = filename(exe_path.text, false);
	path_t app_name = appname(exe_path.text);
	title_t window_title = title(hwnd);
	void *hico = icon(exe_path.text, hwnd);

	linked_window_t *window = &windows->array[windows->num_windows];
	window->exe_path = exe_path;
	window->exe_name = exe_name;
	window->app_name = app_name;
	window->title = window_title;
	window->hico = hico;
	window->hwnd = hwnd;
	window->next_sub_window = NULL;
	window->prev_sub_window = NULL;
	window->next_top_window = NULL;
	window->prev_top_window = NULL;

	windows->num_windows++;

	if (windows->num_windows == 1) {
		windows->num_apps = 1; // num_apps is incremented below but needs to be init'd here
	}
	if (windows->num_windows < 2) {
		return; // Skip linking when there is only 1 or 0 windows
	}

	// Check if new window is same app as previous window and link them if so
	linked_window_t *window1 = &windows->array[0];
	linked_window_t *window2 = window;
	while (window1) {
		if (patheq(&window1->exe_path, &window2->exe_path)) {
			while (window1->next_sub_window) { window1 = window1->next_sub_window; } // Get last subwindow of window1
			window1->next_sub_window = window2;
			window2->prev_sub_window = window1;
			break;
		}
		if (window1->next_top_window) {
			window1 = window1->next_top_window; // Check next top window
		} else {
			break;
		}
	}
	// window2 was not assigned a previous window of the same app, thus window2 is a top window
	if (window2->prev_sub_window == NULL) {
		window2->prev_top_window = window1;
		window1->next_top_window = window2;
		windows->num_apps++;
	}
}

static void restart(windows_t *windows) {
	for (int i = 0; i < windows->num_windows; i++) {
		if (windows->array[i].hico) {
			DestroyIcon(windows->array[i].hico);
		}
	}
	windows->num_windows = 0;
	windows->num_apps = 0;

	// Rebuild our whole window list
	EnumWindows(EnumProc, 0); // EnumProc calls filter and add
}

static BOOL EnumProc(HWND hWnd, LPARAM lParam) {
	if (filter(hWnd)) {
		add(&windows, hWnd);
	}
	return true; // Return true to continue EnumWindows
}


static void print_hwnd(void *hwnd, bool details) {
	if (hwnd) {
		path_t exe_path = path(hwnd);
		path_t exe_name = filename(exe_path.text, false);
		title_t window_title = title(hwnd);
		wchar_t *ws_iconic = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_ICONIC) ? L" (minimized)" : L"";
		print(L"%s%s \"%s\"\n", exe_name.text, ws_iconic,  window_title.text, exe_path.text);
		if (details) {
			wchar_t *visible             = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_VISIBLE) ? L"visible" : L"";
			wchar_t *child               = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) ? L", child" : L"";
			wchar_t *disabled            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DISABLED) ? L", disabled" : L"";
			wchar_t *popup               = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUP) ? L", popup" : L"";
			wchar_t *popupwindow         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_POPUPWINDOW) ? L", popupwindow" : L"";
			wchar_t *tiled               = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_TILED) ? L", tiled" : L"";
			wchar_t *tiledwindow         = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_TILEDWINDOW) ? L", tiledwindow" : L"";
			wchar_t *overlapped          = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPED) ? L", overlapped" : L"";
			wchar_t *overlappedwindow    = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW) ? L", overlappedwindow" : L"";
			wchar_t *dlgframe            = (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_DLGFRAME) ? L", dlgframe" : L"";
			wchar_t *ex_toolwindow       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) ? L", ex_toolwindow" : L"";
			wchar_t *ex_appwindow        = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_APPWINDOW) ? L", ex_appwindow" : L"";
			wchar_t *ex_dlgmodalframe    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_DLGMODALFRAME) ? L", ex_dlgmodalframe" : L"";
			wchar_t *ex_layered          = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) ? L", ex_layered" : L"";
			wchar_t *ex_noactivate       = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) ? L", ex_noactivate" : L"";
			wchar_t *ex_palettewindow    = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_PALETTEWINDOW) ? L", ex_palettewindow" : L"";
			wchar_t *ex_overlappedwindow = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_OVERLAPPEDWINDOW) ? L", ex_overlappedwindow" : L"";

			wchar_t class_name[MAX_PATH] = {0};
			GetClassNameW(hwnd, class_name, countof(class_name));

			print(L"\t");
			print(L"%s (%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s) ",
				class_name,
				visible, child, disabled, popup, popupwindow, tiled, tiledwindow, overlapped, overlappedwindow, dlgframe,
				ex_toolwindow, ex_appwindow, ex_dlgmodalframe, ex_layered, ex_noactivate, ex_palettewindow, ex_overlappedwindow);
		}		
	} else {
		print(L"NULL hwnd\n");
	}
}

static void print_window(linked_window_t *window) {
	if (window && window->hwnd) {
		wchar_t *ws_iconic = (GetWindowLongPtrW(window->hwnd, GWL_STYLE) & WS_ICONIC) ? L" (minimized)" : L"";
		print(L"%s%s \"%s\"\n", window->exe_name.text, ws_iconic,  window->title.text, window->exe_path.text);
	} else {
		print(L"NULL window\n");
	}
}

static void print_all(windows_t *windows, bool fancy) {
	double mem = (double)(
		sizeof *windows +
		sizeof selection +
		sizeof keyboard +
		//sizeof style + // TDODO Depends on whether style is in config_t or not
		sizeof gdi +
		sizeof config) / 1024; // TODO Referencing global vars
	print(L"%i Alt-Tab windows (%.0fkb mem):\n", windows->num_windows, mem);

	if (!fancy) {
		// Just dump the array
		for (int i = 0; i < windows->num_windows; i++) {
			linked_window_t *window = &windows->array[i];
			print(L"%i ", i);
			print_window(window);
		}
	} else {
		// Dump the linked list
		linked_window_t *top_window = &windows->array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
		linked_window_t *sub_window = top_window->next_sub_window;
		while (top_window) {
			print(L"+ ");
			print_window(top_window);
			while (sub_window) {
				print(L"\t- ");
				print_window(sub_window);
				sub_window = sub_window->next_sub_window;
			}
			if (top_window->next_top_window) {
				top_window = top_window->next_top_window;
				sub_window = top_window->next_sub_window;
			} else {
				break;
			}
		}
	}
	print(L"\n");
	print(L"________________________________\n");
}


static linked_window_t *find_first(linked_window_t *window, bool top_level) {
	if (top_level) {
		while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
		while (window->prev_top_window) { window = window->prev_top_window; } // 2. Go to first top widndow
	} else {
		while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window (which is also technically "first sub window")
	}
	return window; // TODO WTF How does this work in next() without return value!?
}

static linked_window_t *find_last(linked_window_t *window, bool top_level) {
	if (top_level) {
		while (window->prev_sub_window) { window = window->prev_sub_window; } // 1. Go to top window
		while (window->next_top_window) { window = window->next_top_window; } // 2. Go to last top window
	} else {
		while (window->next_sub_window) { window = window->next_sub_window; } // 1. Go to last sub window
	}
	return window; // TODO WTF How does this work in next() without return value!?
}

static linked_window_t *find_next(linked_window_t *window, bool top_level, bool reverse_direction, bool allow_wrap) {
	if (window == NULL) {
		print(L"window is NULL");
		return NULL;
	}

	if (top_level) {
		// Ensure that 'window' is its app's top window
		window = find_first(window, false);
	}

	linked_window_t *next_window = NULL;

	if (top_level) {
		next_window = reverse_direction ? window->prev_top_window : window->next_top_window;
	} else {
		next_window = reverse_direction ? window->prev_sub_window : window->next_sub_window;
	}

	if (next_window) {
		return next_window;
	}

	// No next window. Should we wrap around?

	if (!allow_wrap) { 
		return window;
	}

	bool alone;

	if (top_level) {
		// Are we the only top level window? All alone on the top :(
		alone = !window->next_top_window && !window->prev_top_window; 
	} else {
		// Are we the only window of this app? Top looking for sub ;)
		alone = !window->next_sub_window && !window->prev_sub_window; 
	}

	if (!alone) { // '!next_window && allow_wrap' implied
		return reverse_direction ? find_last(window, top_level) : find_first(window, top_level);
	}

	return window;
}


static void select_foreground(selection_t *selection) {
	selection->current = windows.num_windows > 0 ? &windows.array[0] : NULL;
	selection->restore = GetForegroundWindow();

	if (selection->current == NULL) {
		// The foreground window did not pass filter
	}
	
	if (selection->current && selection->current->hwnd != selection->restore)  {
		// The restore window did not pass filter
	}
}

static void select_next(selection_t *selection, bool top_level, bool reverse_direction, bool allow_wrap) {
	selection->current = find_next(selection->current, top_level, reverse_direction, allow_wrap);
}

static void select_show(selection_t *selection) {
	selection->showed = selection->current;
	show_hwnd(selection->showed->hwnd, true);
}


static void cancel(selection_t *selection, void *hwnd, bool restore)  {
	// NOTE We strive to keep selection state and gui state independent, but in the
	//      case of cancellation there is a dependence between the two, encapsulated
	//      in this function, illustrated by this function having both a selection_t
	//      parameter and a hwnd parameter.
	//      The dependence is basically that when selection is cancelled, we want the
	//      gui to disappear, and that when the window is externally deactivated we
	//      want the selection to reset.
	//      (See WndProc case WM_ACTIVATE for the other side of this dependence.)

	KillTimer(hwnd, 1);
	ShowWindow(hwnd, SW_HIDE); // Yes, call ShowWindow() to hide window...

	if (restore) {
		show_hwnd(selection->restore, true);
	}

	selection->current = NULL;
	selection->showed = NULL;
	selection->restore = NULL;
}


static void draw_gui(void *hwnd, gdi_t *gdi, style_t *style, selection_t *selection) {

	RECT rect = {0};
	GetClientRect(hwnd, &rect);

	if (!EqualRect(&gdi->rect, &rect)) {

		gdi->rect = rect;
		gdi->width = rect.right - rect.left;
		gdi->height = rect.bottom - rect.top;

		//DeleteObject(gdi->hbm);
		DeleteDC(gdi->hdc);

		HDC hdc = GetDC(hwnd);
		gdi->hdc = CreateCompatibleDC(hdc);
		gdi->hbm = CreateCompatibleBitmap(hdc, gdi->width, gdi->height);
		HBITMAP old_hbm = (HBITMAP)SelectObject(gdi->hdc, gdi->hbm);
		//DeleteObject(old_hbm);
	}

	// TODO Use 'style_t' 

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

	// Invalidate & draw window background
	RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
	FillRect(gdi->hdc, &rect, window_background);

	// Select pen & brush for RoundRect() (used to draw selection rectangle)
	HPEN old_pen = (HPEN)SelectObject(gdi->hdc, selection_outline);
	HBRUSH old_brush = (HBRUSH)SelectObject(gdi->hdc, selection_background);

	// Select text font, color & background for DrawTextW() (used to draw app name)
	HFONT old_font = (HFONT)SelectObject(gdi->hdc, GetStockObject(DEFAULT_GUI_FONT));
	SetTextColor(gdi->hdc, TEXT_COLOR);
	SetBkMode(gdi->hdc, TRANSPARENT);

	// This is how GDI handles memory?
	//DeleteObject(old_pen);
	//DeleteObject(old_brush);
	//DeleteObject(old_font);


	int i = 0;
	linked_window_t *window = &windows.array[0]; // windows->array[0] is always a top window because it is necessarily the first window of its app in the list
	while (window) {
		linked_window_t *top1 = find_first(window, false);
		linked_window_t *top2 = find_first(selection->current, false);
		bool app_is_selected = (top1 == top2);

		// Special-case math for index 0
		int  left0 = i == 0 ? ICON_PAD : 0;
		int right0 = i != 0 ? ICON_PAD : 0;

		int  icons = (ICON_PAD + ICON_WIDTH + ICON_PAD) * i;
		int   left = HORZ_PAD + left0 + icons + right0;
		int    top = VERT_PAD;
		int  width = ICON_WIDTH;
		int height = ICON_WIDTH;
		int  right = left + width;
		int bottom = top + height;

		if (!app_is_selected) {
			// Draw only icon, with window background
			DrawIconEx(gdi->hdc, left, top, window->hico, width, height, 0, window_background, DI_NORMAL);
		} else {
			// Draw selection rectangle and icon, with selection background
			RoundRect(gdi->hdc, left - SEL_VERT_OFF, top - SEL_HORZ_OFF, right + SEL_VERT_OFF, bottom + SEL_HORZ_OFF, SEL_RADIUS, SEL_RADIUS);
			DrawIconEx(gdi->hdc, left, top, window->hico, width, height, 0, selection_background, DI_NORMAL);
			
			// Draw app name
			wchar_t *title;
			if (window->app_name.ok) {
				title = window->app_name.text;
			} else {
				title = window->exe_name.text;
			}

			// Measure text width
			RECT test_rect = {0};
			DrawTextW(gdi->hdc, title, -1, &test_rect, DT_CALCRECT | DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
			int title_width = (test_rect.right - test_rect.left) + 2; // Hmm, measured size seems a teensy bit too small, so +2

			// DrawTextW() uses a destination rect for drawing
			RECT text_rect = rect;

			int width_diff = (right - left) - title_width;
			if (width_diff < 0) {
				// text_rect is too small for text, grow the rect
				text_rect.left = left + ceilf(width_diff / 2);
				text_rect.right = right - ceilf(width_diff / 2);
			} else {
				text_rect.left = left;
				text_rect.right = right;
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

			///* dbg */ FillRect(gdi->hdc, &text_rect, CreateSolidBrush(RGB(255, 0, 0))); // Check the size of the text box
			DrawTextW(gdi->hdc, title, -1, &text_rect, DT_SINGLELINE | DT_CENTER | DT_BOTTOM);
		}

		// Go to draw next linked window
		if (window->next_top_window) {
			window = window->next_top_window;
			i++;
		} else {
			break;
		}
	}
}


static bool get_key(keyboard_t *keyboard, vkcode key) {
	return BITTEST(keyboard->keys, key);
}

static bool set_key(keyboard_t *keyboard, vkcode key, bool key_down) {
	bool already_down = get_key(keyboard, key);
	if (key_down) {
		BITSET(keyboard->keys, key);
	} else {
		BITCLEAR(keyboard->keys, key);
	}
	return already_down && key_down; // Return whether this was a key repeat
}


static void autorun(bool enabled, wchar_t *reg_key_name, wchar_t (*launch_args)[MAX_PATH]) {

	// Assemble the target (filepath + args) for the autorun reg key

	wchar_t filepath[MAX_PATH];
	wchar_t target[sizeof filepath + sizeof *launch_args];

	GetModuleFileNameW(NULL, filepath, sizeof filepath / sizeof *filepath); // Get filepath of current module
	StringCchPrintfW(target, sizeof target / sizeof *target, L"\"%s\" %s", filepath, *launch_args); // "sprintf"

	HKEY reg_key;
	LSTATUS success; // BUG Not checking 'success' below

	if (enabled) {
		success = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, 0, KEY_SET_VALUE, NULL, &reg_key, NULL);
		success = RegSetValueExW(reg_key, reg_key_name, 0, REG_SZ, target, sizeof target);
		success = RegCloseKey(reg_key);
	} else {
		success = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &reg_key);
		success = RegDeleteValueW(reg_key, reg_key_name);
		success = RegCloseKey(reg_key);
	}
}


static intptr_t LLKeyboardProc(int nCode, intptr_t wParam, intptr_t lParam) {
	if (nCode < 0 || nCode != HC_ACTION) { // "If code is less than zero, the hook procedure must pass the message to blah blah blah read the docs"
		return CallNextHookEx(NULL, nCode, wParam, lParam);
	}

	// Info about the keyboard for the low level hook
	KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;
	vkcode virtual_key = kb->vkCode;
	bool key_down = !(kb->flags & LLKHF_UP);
	bool key_repeat = set_key(&keyboard, virtual_key, key_down); // Manually track key repeat using 'keyboard->keys' (see above)

	// Typically:
	//  mod1+key1 = Alt+Tab
	//  mod2+key2 = Alt+`   (scancode 0x29)
	// TODO Remove mod2 ? Just have one modifier ?
	bool shift_held  = get_key(&keyboard, VK_LSHIFT); // NOTE This is the *left* shift key
	bool mod1_held   = get_key(&keyboard, config.mod1);
	bool mod2_held   = get_key(&keyboard, config.mod2);
	bool key1_down   = virtual_key == config.key1 && key_down;
	bool key2_down   = virtual_key == config.key2 && key_down;
	bool mod1_up     = virtual_key == config.mod1 && !key_down;
	bool mod2_up     = virtual_key == config.mod2 && !key_down;
	bool esc_down    = virtual_key == VK_ESCAPE && key_down;

	// Extended window management functionality
	// TODO Separate up/left & down/right: Use left/right for apps & up/down for windows.
	//      Especially since I think we're eventually gonna graphically put a vertical
	//      window title list under the horizontal app icon list
	bool enter_down  = virtual_key == VK_RETURN && key_down;
	bool delete_down = virtual_key == VK_DELETE && key_down;
	bool prev_down   = (virtual_key == VK_UP || virtual_key == VK_LEFT) && key_down;
	bool next_down   = (virtual_key == VK_DOWN || virtual_key == VK_RIGHT) && key_down;
	bool ctrl_held   = get_key(&keyboard, VK_LCONTROL); // NOTE This is the *left* ctrl key // BUG! What if the users chooses their mod1 or mod2 to be ctrl?
	bool keyQ_down   = virtual_key ==  0x51 && key_down;
	bool keyW_down   = virtual_key ==  0x57 && key_down;
	bool keyM_down  =  virtual_key ==  0x4D && key_down;
	bool keyF4_down  = virtual_key == VK_F4 && key_down;

	// NOTE By returning a non-zero value from the hook procedure, the message
	//      is consumed and does not get passed to the target window
	intptr_t pass_message = 0;
	intptr_t consume_message = 1;

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
		bool enabled = config.switch_apps = !config.switch_apps; // NOTE Atm, we only support a hotkey for toggling app switching, not toggling window switching (cuz why would we?)
		if (!enabled) {
			cancel(&selection, hMainWindow, false);
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
	if (!selection.current && key1_down && mod1_held && config.switch_apps) {
		restart(&windows);
		select_foreground(&selection); // Sets 'selection.current'
		// NOTE No return, as the two activations fallthrough to navigation below
	}
	
	// First Alt+`
	if (!selection.current && key2_down && mod2_held && config.switch_windows) {
		restart(&windows);
		select_foreground(&selection); // Sets 'selection.current'
		select_next(&selection, false, shift_held, !config.wrap_bump || !key_repeat); // TODO / BUG Could potentially leave selection->current == NULL
		// NOTE No return, as the two activations fallthrough to navigation below
	}

	// ========================================
	// Navigation
	// ========================================
	
	// Alt+Tab - next app (hold Shift for previous)
	if (selection.current && key1_down && mod1_held) {
		select_next(&selection, true, shift_held, !config.wrap_bump || !key_repeat); // TODO / BUG Could potentially leave selection->current == NULL
		if (config.fast_switching_apps) {
			select_show(&selection);
		}
		if (config.gui_for_apps) {
			size_gui(hMainWindow, &config.style, windows.num_apps); // NOTE Must call size_gui() after restart(), before draw_gui() & before show_gui()
			draw_gui(hMainWindow, &gdi, &config.style, &selection);
			show_gui(hMainWindow, config.gui_delay);
		}
		//intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
		return consume_message; // Since we "own" this hotkey system-wide, we consume this message unconditionally
	}

	// Alt+` - next window (hold Shift for previous)
	if (selection.current && key2_down && mod2_held) {
		if (selection.showed || selection.showed == selection.current) {
			select_next(&selection, false, shift_held, !config.wrap_bump || !key_repeat); // TODO / BUG Could potentially leave selection->current == NULL
			//print(L"Alt+`ing after having alt+`ed\n");
		} else {
			//print(L"Alt+`ing after having alt+tab'ed\n");
		}

		if (config.fast_switching_windows) {
			select_show(&selection);
		}
		if (config.gui_for_windows) {
			size_gui(hMainWindow, &config.style, windows.num_apps); // NOTE Must call size_gui() after restart(), before draw_gui() & before show_gui()
			draw_gui(hMainWindow, &gdi, &config.style, &selection);
			show_gui(hMainWindow, config.gui_delay);
		}
		//intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
		return consume_message; // Since we "own" this hotkey system-wide, we consume this message unconditionally
	}

	// Alt+Arrows. People ask for this, so...
	if (selection.current && (next_down || prev_down) && mod1_held) { // selection.current: You can't *start* window switching with Alt+Arrows, but you can navigate the switcher with arrows when switching is active
		select_next(&selection, true, prev_down, !config.wrap_bump || !key_repeat);
		if (config.fast_switching_apps) {
			select_show(&selection);
			//peek(selection->current->hwnd); // TODO Doesn't work yet. See config.fast_switching and peek()
		}
		if (config.gui_for_apps) {
			// Update selection highlight
			draw_gui(hMainWindow, &gdi, &config.style, &selection);
		}

		//intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
		return consume_message; // Since our swicher is active, we "own" this hotkey, so we consume this message
	}

	// ========================================
	// Deactivation
	// ========================================

	// Alt keyup - switch to selected window
	// BUG! Aha! Lemme tell yalittle secret: Our low level hook doesn't observe "Alt keydown" because Alt can be pressed down for
	//      so many purposes on system level. We're only interested in Alt+Tab, which means that we only really check for "Tab keydown",
	//      and then transparently check if Alt was already down when Tab was pressed.
	//      This means that we let all "Alt keydown"s pass through our hook, even the "Alt keydown" that the user pressed right before
	//      pressing Tab, which is sort of "ours", but we passed it through anyways (can't consume it after-the-fact).
	//      The result is that there is a hanging "Alt down" that got passed to the foreground app which needs an "Alt keyup", or
	//      the Alt key gets stuck. Thus, we always pass through "Alt keyup"s, even if we used it for some functionality.
	//      In short:
	//      1) Our app never hooks Alt keydowns
	//      2) Our app hooks Alt keyups, but always passes it through on pain of getting a stuck Alt key because of point 1
	if (selection.current && (mod1_up || mod2_up)) {
		select_show(&selection);
		cancel(&selection, hMainWindow, false);
		return pass_message; // See note above on "Alt keyup" on why we don't consume this message
	}
	// Alt+Enter - switch to selected window
	if (selection.current && (mod1_held || mod2_held) && enter_down) {
		select_show(&selection);
		cancel(&selection, hMainWindow, false);
		return pass_message; // See note above on "Alt keyup" on why we don't consume this message
	}
	// Alt+Esc - cancel switching and restore initial window
	if (selection.current && (mod1_held || mod2_held) && esc_down) { // BUG There's a bug here if mod1 and mod2 aren't both the same key. In that case we'll still pass through the key event, even though we should own it since it's not Alt+Esc
		cancel(&selection, hMainWindow, true);
		//intptr_t result = CallNextHookEx(NULL, nCode, wParam, lParam); // We'll be good citizens
		return consume_message; // We don't "own"/monopolize Alt+Esc. It's used by Windows for another, rather useful, window switching mechaninsm, and we preserve that
	}
	// ========================================
	// Extended window management
	// ========================================
	
	// Alt+F4 - close cmdtab
	if (selection.current && ((mod1_held || mod2_held) && keyF4_down)) {
		close(hMainWindow);
	}
	// Alt+Q - close all windows of selected application
	if (selection.current && ((mod1_held || mod2_held) && keyQ_down)) {
		print(L"Q\n");
	}
	// Alt+W or Alt+Delete - close selected window
	if (selection.current && ((mod1_held || mod2_held) && (keyW_down || delete_down))) {
		close(selection.current->hwnd);
		restart(&windows);
		size_gui(hMainWindow, &config.style, windows.num_apps); // NOTE Must call size_gui after restart, before draw_gui & before show_gui
		draw_gui(hMainWindow, &gdi, &config.style, &selection);
	}
	// Alt+M - minimize selected window
	if (selection.current && ((mod1_held || mod2_held) && keyM_down)) {
		minimize(selection.current->hwnd);
	}

	// Input sink - consume keystrokes when activated
	if (selection.current && (mod1_held || mod2_held)) {
		return consume_message;
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static intptr_t WndProc(void *hwnd, unsigned message, intptr_t wParam, intptr_t lParam) {
	switch (message) {
		case WM_ERASEBKGND: {
			return 1;
		}
		case WM_ACTIVATE: {
			if (wParam == WA_INACTIVE) {
				//print(L"gui lost focus\n");
				cancel(&selection, hMainWindow, false); // NOTE See comments in cancel()
			} 
			return DefWindowProcW(hwnd, message, wParam, lParam);
		}
		case WM_PAINT: {
			// Double buffered drawing:
			// We draw off-screen in-memory to 'gdi.hdc' whenever changes occur,
			// and whenever we get the WM_PAINT message we just blit the
			// off-screen buffer to screen ("screen" means the 'hdc' returned by
			// BeginPaint() passed our 'hwnd')
			PAINTSTRUCT ps = {0};
			HDC hdc = BeginPaint(hwnd, &ps);
			BitBlt(hdc, 0, 0, gdi.width, gdi.height, gdi.hdc, 0, 0, SRCCOPY);
			//int scale = 1.0;
			//SetStretchBltMode(gdi.hdc, HALFTONE);
			//StretchBlt(hdc, 0, 0, gdi.width, gdi.height, gdi.hdc, 0, 0, gdi.width * scale, gdi.height * scale, SRCCOPY);
			EndPaint(hwnd, &ps);
			return 0;
		}
		case WM_CLOSE: {
			cancel(&selection, hMainWindow, false);
			int result = MessageBoxW(NULL, L"Do you want to quit cmdtab?", L"cmdtab", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL);
			switch (result) {
				case IDYES: return DefWindowProcW(hwnd, message, wParam, lParam);
				case IDNO: return 0;
			}
		}
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;
		}
		default: {
			return DefWindowProcW(hwnd, message, wParam, lParam);
		}
	}
}

//==============================================================================
// Windows API main()
//==============================================================================

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {

	// Prompt about autorun (unless we got autorun arg)
	if (wcscmp(pCmdLine, L"--autorun")) { // lstrcmpW, CompareStringW
		int result = MessageBoxW(NULL, L"Start cmdtab.exe automatically? (Relaunch cmdtab.exe to change your mind.)", L"cmdtab", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL);
		switch (result) {
			case IDYES: autorun(true, L"cmdtab", &L"--autorun"); break; // BUG Passing wchar_t[10] arg to wchar_t[MAX_PATH] param
			case IDNO: autorun(false, L"cmdtab", &L"--autorun"); break; // BUG Passing wchar_t[10] arg to wchar_t[MAX_PATH] param
		}
	}

	// Quit if another instance of cmdtab is already running
	void *hMutex = CreateMutexW(NULL, TRUE, L"cmdtab_mutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		int result = MessageBoxW(NULL, L"cmdtab.exe is already running.", L"cmdtab", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
		return result; // 0
	}

	// Set default settings
	defaults(&config);

	// Install keyboard hook
	void *hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, (HOOKPROC)LLKeyboardProc, NULL, 0);

	// Create window
	WNDCLASSEX wcex = {0};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)WndProc;
	wcex.hInstance = hInstance;
	wcex.lpszClassName = L"cmdtab";

	hMainWindow = CreateWindowExW(WS_EX_TOOLWINDOW, RegisterClassExW(&wcex), NULL, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

	// Clear all window styles for very plain window
	SetWindowLongW(hMainWindow, GWL_STYLE, 0);

	// Use dark mode to make the title bar dark
	// The alternative is to disable the title bar but this removes nice rounded
	// window shaping.
	// For dark mode details, see: https://stackoverflow.com/a/70693198/659310
	//BOOL USE_DARK_MODE = TRUE;
	//DwmSetWindowAttribute(hMainWindow, DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE, sizeof USE_DARK_MODE);

	// Rounded window corners
	DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
	DwmSetWindowAttribute(hMainWindow, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof preference);

	// Start msg loop for thread
	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0) > 0) {
		DispatchMessageW(&msg);
	}

	// Done
	UnhookWindowsHookEx(hKeyboardHook);
	CloseHandle(hKeyboardHook);
	if (hMutex != 0 && hMutex != NULL) {
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
	}

	return (int)msg.wParam;
}
