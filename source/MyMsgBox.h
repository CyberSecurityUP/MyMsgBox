#pragma once
#ifndef MYMSGBOX_H
#define MYMSGBOX_H

// Keep Windows headers lean and avoid min/max macro conflicts
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

// Custom flags
enum MYMB_FLAGS : unsigned {
    MYMB_OK        = 0x0001,
    MYMB_OKCANCEL  = 0x0002,
    MYMB_YESNO     = 0x0004,

    MYMB_ICON_NONE = 0x0000,
    MYMB_ICON_INFO = 0x0100,
    MYMB_ICON_WARN = 0x0200,
    MYMB_ICON_ERR  = 0x0400,

    MYMB_DEFAULT2  = 0x1000, // second button is default (Enter)
    MYMB_TOPMOST   = 0x2000  // keep window on top
};

// Export / import macro for DLL
#ifdef MYMSGBOXLIB_EXPORTS
  #define MYMSG_API __declspec(dllexport)
#else
  #define MYMSG_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Public API: returns IDOK / IDCANCEL / IDYES / IDNO
MYMSG_API int MyMessageBox(HWND owner,
                           const wchar_t* text,
                           const wchar_t* title,
                           unsigned flags);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MYMSGBOX_H