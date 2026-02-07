// efi/efi.h
// Main EFI header - includes all our custom UEFI definitions
//
// This is our "from scratch" UEFI library, written from the spec.
// We use gnu-efi only for:
//   - uefi_call_wrapper() (ABI conversion thunk)
//   - crt0-efi-x86_64.o (entry point)
//   - elf_x86_64_efi.lds (linker script)
//
// Everything else - types, tables, protocols - we define ourselves.
#pragma once

#include "types.h"
#include "tables.h"
#include "protocols/text.h"
#include "protocols/graphics.h"
#include "protocols/file.h"

//=============================================================================
// ACPI Configuration Table GUIDs (UEFI Spec 4.6)
// Used to find the RSDP (Root System Description Pointer)
//=============================================================================

// ACPI 2.0+ RSDP
#define EFI_ACPI_20_TABLE_GUID \
    EFI_GUID_VALUE(0x8868e871, 0xe4f1, 0x11d3, \
                   0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81)

// ACPI 1.0 RSDP (fallback)
#define EFI_ACPI_TABLE_GUID \
    EFI_GUID_VALUE(0xeb9d2d30, 0x2d88, 0x11d3, \
                   0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d)

//=============================================================================
// Helper Macros
//=============================================================================

// Calculate the number of pages needed for a byte count
#define EFI_SIZE_TO_PAGES(size)  (((size) + 4095) >> 12)

// Calculate bytes from page count
#define EFI_PAGES_TO_SIZE(pages) ((pages) << 12)
