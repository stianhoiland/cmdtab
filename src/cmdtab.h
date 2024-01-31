#pragma once

// header.h : include file for standard system include files,
// or project specific include files
//

#define _UNICODE
#define UNICODE
//#include "resource.h"
#include "targetver.h"
// Windows Header Files
//#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>
#include <memory.h>
//#include <tchar.h>


//
// cmdtab.c
//
#pragma comment(lib, "shlwapi.lib") 
#pragma comment(lib, "dwmapi.lib") 
#pragma comment(lib, "version.lib") // Used by appname()
//#pragma comment(lib,"user32.lib") 

#include <wchar.h>     // We're wiiiide, baby!
#include <stdarg.h>    // Used by print()
//#include "debugapi.h"  // Used by print() -- ehh: "To use this function [OutputDebugStringW], you must include the Windows.h header in your application (not debugapi.h)." (docs)
#include "WinUser.h"   // Used by title(), path(), filter(), link(), next()
#include "processthreadsapi.h" // Used by path()
#include "WinBase.h"   // Used by path()
#include "handleapi.h" // Used by path()
#include "Shlwapi.h"   // Used by name()
//#include "corecrt_wstring.h" // Used by name(), included by wchar.h
#include "dwmapi.h"    // Used by filter()
#define COBJMACROS
#include "commoncontrols.h" // Used by icon()

//#include "winver.h" // Used by appname()

#include "strsafe.h" // Used by appname()



#include <intrin.h> // Used by debug(), crash()

#define debug() __debugbreak() // Or DebugBreak() in debugapi.h
#define crash() __fastfail(FAST_FAIL_FATAL_APP_EXIT)
#define noop() while(0)

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






//#include "guiddef.h"

//#include <commoncontrols.h>
//#include "comip.h"
//#include "comdef.h"

//#include "combaseapi.h" // IID_PPV_ARGS macro
//#include "commctrl.h"
//#include "dpa_dsa.h"
//#include "prsht.h"
//#include "richedit.h"
//#include "richole.h"
//#include "shlobj.h"
//#include "textserv.h"
//#include "tom.h"
//#include "uxtheme.h"
//#include "windowsx.h"
//#include "winuser.h"

//#pragma comment(lib, "Comctl32.lib") 
