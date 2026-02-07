// efi/protocols/file.h
// Simple File System and File Protocol from UEFI Specification 2.10
// Section 13.4 (Simple File System) and Section 13.5 (File Protocol)
#pragma once

#include "../types.h"

// Forward declarations
struct _EFI_FILE_PROTOCOL;
struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

//=============================================================================
// GUIDs
//=============================================================================

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    EFI_GUID_VALUE(0x0964e5b22, 0x6459, 0x11d2, \
                   0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

#define EFI_FILE_INFO_GUID \
    EFI_GUID_VALUE(0x09576e92, 0x6d3f, 0x11d2, \
                   0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b)

//=============================================================================
// File Open Modes and Attributes
//=============================================================================

#define EFI_FILE_MODE_READ    0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE   0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE  0x8000000000000000ULL

#define EFI_FILE_READ_ONLY  0x0000000000000001ULL
#define EFI_FILE_HIDDEN     0x0000000000000002ULL
#define EFI_FILE_SYSTEM     0x0000000000000004ULL
#define EFI_FILE_RESERVED   0x0000000000000008ULL
#define EFI_FILE_DIRECTORY  0x0000000000000010ULL
#define EFI_FILE_ARCHIVE    0x0000000000000020ULL

//=============================================================================
// EFI Time Structure
//=============================================================================

typedef struct {
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
    UINT8  Pad1;
    UINT32 Nanosecond;
    INT16  TimeZone;
    UINT8  Daylight;
    UINT8  Pad2;
} EFI_TIME;

//=============================================================================
// File Info Structure
//=============================================================================

typedef struct {
    UINT64   Size;
    UINT64   FileSize;
    UINT64   PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64   Attribute;
    CHAR16   FileName[];
} EFI_FILE_INFO;

//=============================================================================
// File Protocol Function Pointer Types
//=============================================================================

typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(
    struct _EFI_FILE_PROTOCOL  *This,
    struct _EFI_FILE_PROTOCOL **NewHandle,
    CHAR16                    *FileName,
    UINT64                     OpenMode,
    UINT64                     Attributes
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(
    struct _EFI_FILE_PROTOCOL *This
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_DELETE)(
    struct _EFI_FILE_PROTOCOL *This
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_READ)(
    struct _EFI_FILE_PROTOCOL *This,
    UINTN                     *BufferSize,
    VOID                      *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_WRITE)(
    struct _EFI_FILE_PROTOCOL *This,
    UINTN                     *BufferSize,
    VOID                      *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_POSITION)(
    struct _EFI_FILE_PROTOCOL *This,
    UINT64                    *Position
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_POSITION)(
    struct _EFI_FILE_PROTOCOL *This,
    UINT64                     Position
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_INFO)(
    struct _EFI_FILE_PROTOCOL *This,
    EFI_GUID                  *InformationType,
    UINTN                     *BufferSize,
    VOID                      *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_INFO)(
    struct _EFI_FILE_PROTOCOL *This,
    EFI_GUID                  *InformationType,
    UINTN                      BufferSize,
    VOID                      *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_FLUSH)(
    struct _EFI_FILE_PROTOCOL *This
);

//=============================================================================
// File Protocol
//=============================================================================

typedef struct _EFI_FILE_PROTOCOL {
    UINT64               Revision;
    EFI_FILE_OPEN        Open;
    EFI_FILE_CLOSE       Close;
    EFI_FILE_DELETE      Delete;
    EFI_FILE_READ        Read;
    EFI_FILE_WRITE       Write;
    EFI_FILE_GET_POSITION GetPosition;
    EFI_FILE_SET_POSITION SetPosition;
    EFI_FILE_GET_INFO    GetInfo;
    EFI_FILE_SET_INFO    SetInfo;
    EFI_FILE_FLUSH       Flush;
} EFI_FILE_PROTOCOL;

//=============================================================================
// Simple File System Protocol Function Pointer Types
//=============================================================================

typedef EFI_STATUS (EFIAPI *EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)(
    struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    EFI_FILE_PROTOCOL                      **Root
);

//=============================================================================
// Simple File System Protocol
//=============================================================================

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64                                      Revision;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume;
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
