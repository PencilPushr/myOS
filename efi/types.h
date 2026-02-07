// efi/types.h
// Fundamental UEFI types from UEFI Specification 2.10, Section 2.3
// Written from scratch based on the spec
#pragma once

#include <stdint.h>

//=============================================================================
// Basic Types (UEFI Spec 2.3.1)
//=============================================================================

typedef uint8_t   UINT8;
typedef uint16_t  UINT16;
typedef uint32_t  UINT32;
typedef uint64_t  UINT64;
typedef int8_t    INT8;
typedef int16_t   INT16;
typedef int32_t   INT32;
typedef int64_t   INT64;

typedef UINT8     BOOLEAN;
typedef UINT16    CHAR16;     // UCS-2 character
typedef void      VOID;

// Native width types (64-bit on x64)
typedef UINT64    UINTN;
typedef INT64     INTN;

// Pointer and handle types
typedef VOID     *EFI_HANDLE;
typedef VOID     *EFI_EVENT;
typedef UINT64    EFI_STATUS;
typedef UINT64    EFI_TPL;
typedef UINT64    EFI_LBA;
typedef UINT64    EFI_PHYSICAL_ADDRESS;
typedef UINT64    EFI_VIRTUAL_ADDRESS;

#define TRUE  1
#define FALSE 0
#define NULL  ((void *)0)

//=============================================================================
// Status Codes (UEFI Spec Appendix D)
//=============================================================================

#define EFI_SUCCESS               0ULL

// Error codes have high bit set
#define EFI_ERROR_BIT             (1ULL << 63)
#define EFI_ERROR(status)         ((status) & EFI_ERROR_BIT)

#define EFI_LOAD_ERROR            (EFI_ERROR_BIT | 1)
#define EFI_INVALID_PARAMETER     (EFI_ERROR_BIT | 2)
#define EFI_UNSUPPORTED           (EFI_ERROR_BIT | 3)
#define EFI_BAD_BUFFER_SIZE       (EFI_ERROR_BIT | 4)
#define EFI_BUFFER_TOO_SMALL      (EFI_ERROR_BIT | 5)
#define EFI_NOT_READY             (EFI_ERROR_BIT | 6)
#define EFI_DEVICE_ERROR          (EFI_ERROR_BIT | 7)
#define EFI_WRITE_PROTECTED       (EFI_ERROR_BIT | 8)
#define EFI_OUT_OF_RESOURCES      (EFI_ERROR_BIT | 9)
#define EFI_NOT_FOUND             (EFI_ERROR_BIT | 14)

//=============================================================================
// GUID Structure (UEFI Spec 2.3.1)
//=============================================================================

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

// Helper macro to define GUIDs inline
#define EFI_GUID_VALUE(d1, d2, d3, d4_0, d4_1, d4_2, d4_3, d4_4, d4_5, d4_6, d4_7) \
    { d1, d2, d3, { d4_0, d4_1, d4_2, d4_3, d4_4, d4_5, d4_6, d4_7 } }

//=============================================================================
// Calling Convention
//=============================================================================

// UEFI on x64 uses Microsoft ABI, not System V
// This attribute tells GCC to use MS ABI for the function
#define EFIAPI __attribute__((ms_abi))

//=============================================================================
// UEFI Call Wrapper
//=============================================================================

// UEFI uses Microsoft x64 ABI, GCC defaults to System V ABI.
// Since all our function pointer types are marked with EFIAPI (__attribute__((ms_abi))),
// the compiler will generate the correct calling convention automatically.
// We just need a wrapper that passes through to the function.
//
// The 'nargs' parameter is ignored - it exists for compatibility with gnu-efi code.
#define uefi_call_wrapper(func, nargs, ...) ((func)(__VA_ARGS__))

//=============================================================================
// GUID Comparison Helper
//=============================================================================

static inline BOOLEAN guid_equal(const EFI_GUID *a, const EFI_GUID *b) {
    // Compare as two 64-bit values for speed
    const UINT64 *pa = (const UINT64 *)a;
    const UINT64 *pb = (const UINT64 *)b;
    return (pa[0] == pb[0]) && (pa[1] == pb[1]);
}
