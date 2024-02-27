#pragma once

#define _UNICODE
#define UNICODE
//#define _DEBUG


// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
//#include <SDKDDKVer.h>

// Windows Header Files
//#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <stdbool.h>
#include <malloc.h>
#include <memory.h>

// cmdtab.h
#pragma comment(lib, "shlwapi.lib") 
#pragma comment(lib, "dwmapi.lib") 
#pragma comment(lib, "version.lib") // Used by appname()

#include <wchar.h>     // We're wiiiide, baby!
#include <stdarg.h>    // Used by print()
#include "WinUser.h"   // Used by title(), path(), filter(), link(), next()
#include "processthreadsapi.h" // Used by path()
#include "WinBase.h"   // Used by path()
#include "handleapi.h" // Used by path()
#include "Shlwapi.h"   // Used by name()
#include "dwmapi.h"    // Used by filter()
#include "strsafe.h"   // Used by appname()

#define COBJMACROS
#include "commoncontrols.h" // Used by icon()

#include <intrin.h> // Used by debug(), crash()




#define WIDE(str) L##str
#define EXPAND(x) WIDE(x)
#define __WFILE__ EXPAND(__FILE__)
#define __WFUNC__ EXPAND(__FUNCTION__)

#ifdef _DEBUG
#define dbg_print(fmt, ...) print(L"%s():%d " fmt, __WFUNC__, __LINE__, __VA_ARGS__);
#else
#define dbg_print(fmt, ...) 
#endif

#define debug()       __debugbreak() // Or DebugBreak() in debugapi.h
#define crash()       __fastfail(FAST_FAIL_FATAL_APP_EXIT)
#define noop()        while(0)
#define countof(a)    (ptrdiff_t)(sizeof(a) / sizeof(*(a)))

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
