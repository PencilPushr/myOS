// efi/protocols/graphics.h
// Graphics Output Protocol (GOP) from UEFI Specification 2.10, Section 12.9
//
// GOP replaces the older UGA (Universal Graphics Adapter) protocol.
// It provides a simple way to get a framebuffer for graphics output.
#pragma once

#include "../types.h"

// Forward declaration
struct _EFI_GRAPHICS_OUTPUT_PROTOCOL;

//=============================================================================
// GOP GUID
// 9042a9de-23dc-4a38-96fb-7aded080516a
//=============================================================================

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    EFI_GUID_VALUE(0x9042a9de, 0x23dc, 0x4a38, \
                   0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a)

//=============================================================================
// Pixel Formats (UEFI Spec 12.9)
//=============================================================================

typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

//=============================================================================
// Pixel Bitmask (for PixelBitMask format)
//=============================================================================

typedef struct {
    UINT32 RedMask;
    UINT32 GreenMask;
    UINT32 BlueMask;
    UINT32 ReservedMask;
} EFI_PIXEL_BITMASK;

//=============================================================================
// Graphics Mode Information (UEFI Spec 12.9)
//=============================================================================

typedef struct {
    UINT32                    Version;
    UINT32                    HorizontalResolution;
    UINT32                    VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK         PixelInformation;
    UINT32                    PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

//=============================================================================
// GOP Mode Structure (UEFI Spec 12.9)
//=============================================================================

typedef struct {
    UINT32                               MaxMode;
    UINT32                               Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN                                SizeOfInfo;
    EFI_PHYSICAL_ADDRESS                 FrameBufferBase;
    UINTN                                FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

//=============================================================================
// Blt Pixel and Operations
//=============================================================================

typedef struct {
    UINT8 Blue;
    UINT8 Green;
    UINT8 Red;
    UINT8 Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

typedef enum {
    EfiBltVideoFill,
    EfiBltVideoToBltBuffer,
    EfiBltBufferToVideo,
    EfiBltVideoToVideo,
    EfiGraphicsOutputBltOperationMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

//=============================================================================
// Function Pointer Types
//=============================================================================

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE)(
    struct _EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
    UINT32                                ModeNumber,
    UINTN                                *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
);

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE)(
    struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32                               ModeNumber
);

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT)(
    struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *BltBuffer,
    EFI_GRAPHICS_OUTPUT_BLT_OPERATION    BltOperation,
    UINTN SourceX,      UINTN SourceY,
    UINTN DestinationX, UINTN DestinationY,
    UINTN Width,        UINTN Height,
    UINTN Delta
);

//=============================================================================
// Graphics Output Protocol (UEFI Spec 12.9)
//=============================================================================

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE QueryMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE   SetMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT        Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE      *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;
