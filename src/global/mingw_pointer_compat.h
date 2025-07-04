// Compatibility definitions for MinGW for pointer input APIs
// These are typically found in winuser.h in newer Windows SDKs
// but may be missing or incomplete in some MinGW distributions.

#ifndef MINGW_POINTER_COMPAT_H
#define MINGW_POINTER_COMPAT_H

#if defined(__MINGW32__) && !defined(POINTER_INPUT_TYPE_DEFINED) // Add POINTER_INPUT_TYPE_DEFINED to avoid redefinition if some partial header has it

// Check if NTDDI_VERSION is high enough for these APIs, otherwise, they shouldn't be defined.
// CreateSyntheticPointerDevice requires NTDDI_WIN10_RS5 (0x0A000005)
// The project uses NTDDI_VERSION=0x0A000006, which is newer.
#if defined(NTDDI_VERSION) && (NTDDI_VERSION >= 0x0A000005)

// Basic types that should be available from <windows.h> or its prerequisites
// Ensure <windows.h> or necessary underlying headers like <windef.h>, <winnt.h> are included before this file,
// or define them here if strictly necessary (prefer inclusion).
// For DECLARE_HANDLE, POINT, RECT, HANDLE, HWND, DWORD, UINT32, INT32, UINT64, ULONG

// If DECLARE_HANDLE is not available (e.g. <winnt.h> not yet included)
#ifndef DECLARE_HANDLE
#ifdef STRICT
typedef void *HANDLE;
#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
#else
typedef PVOID HANDLE;
#define DECLARE_HANDLE(name) typedef HANDLE name
#endif
#endif

#ifndef BASETYPES
#define BASETYPES
typedef unsigned long ULONG;
typedef ULONG *PULONG;
typedef unsigned short USHORT;
typedef USHORT *PUSHORT;
typedef unsigned char UCHAR;
typedef UCHAR *PUCHAR;
typedef char *PSZ;
#endif  /* !BASETYPES */

// Ensure basic Windows types are defined if not already (simplified check)
// This is risky; ideally, windows.h should provide these.
#ifndef _WINDEF_
// Minimal HRESULT for compilation if not included
#ifndef _HRESULT_DEFINED
#define _HRESULT_DEFINED
typedef long HRESULT;
#endif // _HRESULT_DEFINED
#endif // _WINDEF_

#ifndef _WINNT_
// Minimal definition for APIENTRY if not included
#ifndef APIENTRY
#define APIENTRY __stdcall
#endif
#endif


#define POINTER_INPUT_TYPE_DEFINED // Guard against redefinition

// From: https://learn.microsoft.com/en-us/windows/win32/api/winuser/ne-winuser-tagpointer_input_type
typedef enum tagPOINTER_INPUT_TYPE {
  PT_POINTER  = 1,
  PT_TOUCH    = 2,
  PT_PEN      = 3,
  PT_MOUSE    = 4,
  PT_TOUCHPAD = 5
} POINTER_INPUT_TYPE;

// From: https://learn.microsoft.com/en-us/windows/win32/api/winuser/ne-winuser-pointer_feedback_mode
typedef enum tagPOINTER_FEEDBACK_MODE {
  POINTER_FEEDBACK_DEFAULT  = 1,
  POINTER_FEEDBACK_INDIRECT = 2,
  POINTER_FEEDBACK_NONE     = 3
} POINTER_FEEDBACK_MODE;

// For HSYNTHETICPOINTERDEVICE
DECLARE_HANDLE(HSYNTHETICPOINTERDEVICE);

// From: https://learn.microsoft.com/en-us/windows/win32/inputmsg/pointer-flags-contants
typedef UINT32 POINTER_FLAGS;
#define POINTER_FLAG_NONE         0x00000000U
#define POINTER_FLAG_NEW          0x00000001U
#define POINTER_FLAG_INRANGE      0x00000002U
#define POINTER_FLAG_INCONTACT    0x00000004U
#define POINTER_FLAG_FIRSTBUTTON  0x00000010U
#define POINTER_FLAG_SECONDBUTTON 0x00000020U
#define POINTER_FLAG_THIRDBUTTON  0x00000040U
#define POINTER_FLAG_FOURTHBUTTON 0x00000080U
#define POINTER_FLAG_FIFTHBUTTON  0x00000100U
#define POINTER_FLAG_PRIMARY      0x00002000U
#define POINTER_FLAG_CONFIDENCE   0x00004000U
#define POINTER_FLAG_CANCELED     0x00008000U
#define POINTER_FLAG_DOWN         0x00010000U
#define POINTER_FLAG_UPDATE       0x00020000U
#define POINTER_FLAG_UP           0x00040000U
#define POINTER_FLAG_WHEEL        0x00080000U
#define POINTER_FLAG_HWHEEL       0x00100000U
#define POINTER_FLAG_CAPTURECHANGED 0x00200000U
#define POINTER_FLAG_HASTRANSFORM 0x00400000U

// From: https://learn.microsoft.com/en-us/windows/win32/api/winuser/ne-winuser-pointer_button_change_type
typedef enum tagPOINTER_BUTTON_CHANGE_TYPE {
  POINTER_CHANGE_NONE,
  POINTER_CHANGE_FIRSTBUTTON_DOWN,
  POINTER_CHANGE_FIRSTBUTTON_UP,
  POINTER_CHANGE_SECONDBUTTON_DOWN,
  POINTER_CHANGE_SECONDBUTTON_UP,
  POINTER_CHANGE_THIRDBUTTON_DOWN,
  POINTER_CHANGE_THIRDBUTTON_UP,
  POINTER_CHANGE_FOURTHBUTTON_DOWN,
  POINTER_CHANGE_FOURTHBUTTON_UP,
  POINTER_CHANGE_FIFTHBUTTON_DOWN,
  POINTER_CHANGE_FIFTHBUTTON_UP
} POINTER_BUTTON_CHANGE_TYPE;

// Forward declaration for POINTER_INFO members if not already available via windows.h
// These should come from windef.h typically.
// typedef struct tagPOINT { LONG x; LONG y; } POINT;
// typedef struct tagRECT { LONG left; LONG top; LONG right; LONG bottom; } RECT;

typedef struct tagPOINTER_INFO {
  POINTER_INPUT_TYPE         pointerType;
  UINT32                     pointerId;
  UINT32                     frameId;
  POINTER_FLAGS              pointerFlags;
  HANDLE                     sourceDevice;
  HWND                       hwndTarget;
  POINT                      ptPixelLocation;
  POINT                      ptHimetricLocation;
  POINT                      ptPixelLocationRaw;
  POINT                      ptHimetricLocationRaw;
  DWORD                      dwTime;
  UINT32                     historyCount;
  INT32                      InputData;
  DWORD                      dwKeyStates;
  UINT64                     PerformanceCount;
  POINTER_BUTTON_CHANGE_TYPE ButtonChangeType;
} POINTER_INFO;

typedef UINT32 TOUCH_FLAGS;
#define TOUCH_FLAG_NONE 0x00000000U

typedef UINT32 TOUCH_MASK;
#define TOUCH_MASK_NONE          0x00000000U
#define TOUCH_MASK_CONTACTAREA   0x00000001U
#define TOUCH_MASK_ORIENTATION   0x00000002U
#define TOUCH_MASK_PRESSURE      0x00000004U

typedef struct tagPOINTER_TOUCH_INFO {
  POINTER_INFO pointerInfo;
  TOUCH_FLAGS  touchFlags;
  TOUCH_MASK   touchMask;
  RECT         rcContact;
  RECT         rcContactRaw;
  UINT32       orientation;
  UINT32       pressure;
} POINTER_TOUCH_INFO;

typedef UINT32 PEN_FLAGS;
#define PEN_FLAG_NONE     0x00000000U
#define PEN_FLAG_BARREL   0x00000001U
#define PEN_FLAG_INVERTED 0x00000002U
#define PEN_FLAG_ERASER   0x00000004U

typedef UINT32 PEN_MASK;
#define PEN_MASK_NONE     0x00000000U
#define PEN_MASK_PRESSURE 0x00000001U
#define PEN_MASK_ROTATION 0x00000002U
#define PEN_MASK_TILT_X   0x00000004U
#define PEN_MASK_TILT_Y   0x00000008U

typedef struct tagPOINTER_PEN_INFO {
  POINTER_INFO pointerInfo;
  PEN_FLAGS    penFlags;
  PEN_MASK     penMask;
  UINT32       pressure;
  UINT32       rotation;
  INT32        tiltX;
  INT32        tiltY;
} POINTER_PEN_INFO;

typedef struct tagPOINTER_TYPE_INFO {
  POINTER_INPUT_TYPE type;
  union {
    POINTER_INFO       pointerInfo;
    POINTER_TOUCH_INFO touchInfo;
    POINTER_PEN_INFO   penInfo;
  } Info; // Named the union member for C compatibility
} POINTER_TYPE_INFO, *PPOINTER_TYPE_INFO;

// Function Prototypes - These need to be declared correctly for MinGW
// The WINUSERAPI is __declspec(dllimport) by default in MinGW's windows.h.
// If we are defining them because they are missing, we should declare them as extern.
// However, the issue might be that they *are* declared (hence dllimport error)
// but their parameters use undefined types.
// For now, let's assume these definitions will make the existing declarations work.
// If not, we might need to redeclare them without WINUSERAPI if MinGW's winuser.h is problematic.

// Make sure to include <windows.h> before this header.
// #include <windows.h> // Or at least <windef.h>, <winnt.h> for basic types

#endif // NTDDI_VERSION check
#endif // __MINGW32__ && !POINTER_INPUT_TYPE_DEFINED
#endif // MINGW_POINTER_COMPAT_H
