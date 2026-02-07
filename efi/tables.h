// efi/tables.h
// EFI System Table and Boot Services from UEFI Specification 2.10
// Section 4.3 (EFI System Table) and Section 4.4 (EFI Boot Services)
#pragma once

#include "types.h"

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL  EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_BOOT_SERVICES               EFI_BOOT_SERVICES;
typedef struct _EFI_RUNTIME_SERVICES            EFI_RUNTIME_SERVICES;

//=============================================================================
// Table Header (UEFI Spec 4.2)
// Common header for all UEFI tables
//=============================================================================

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

//=============================================================================
// Configuration Table (UEFI Spec 4.6)
// Used to find ACPI tables, SMBIOS, etc.
//=============================================================================

typedef struct {
    EFI_GUID VendorGuid;
    VOID    *VendorTable;
} EFI_CONFIGURATION_TABLE;

//=============================================================================
// EFI System Table (UEFI Spec 4.3)
// This is the main entry point to all UEFI services
//=============================================================================

typedef struct {
    EFI_TABLE_HEADER                  Hdr;
    CHAR16                           *FirmwareVendor;
    UINT32                            FirmwareRevision;
    EFI_HANDLE                        ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL   *ConIn;
    EFI_HANDLE                        ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *ConOut;
    EFI_HANDLE                        StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *StdErr;
    EFI_RUNTIME_SERVICES             *RuntimeServices;
    EFI_BOOT_SERVICES                *BootServices;
    UINTN                             NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE          *ConfigurationTable;
} EFI_SYSTEM_TABLE;

//=============================================================================
// Memory Types (UEFI Spec 7.2)
//=============================================================================

typedef enum {
    EfiReservedMemoryType,      // 0  - Not usable
    EfiLoaderCode,              // 1  - UEFI application code
    EfiLoaderData,              // 2  - UEFI application data
    EfiBootServicesCode,        // 3  - Boot services code (reclaimable after ExitBootServices)
    EfiBootServicesData,        // 4  - Boot services data (reclaimable after ExitBootServices)
    EfiRuntimeServicesCode,     // 5  - Runtime services code (must preserve)
    EfiRuntimeServicesData,     // 6  - Runtime services data (must preserve)
    EfiConventionalMemory,      // 7  - FREE MEMORY - this is what you want!
    EfiUnusableMemory,          // 8  - Memory with errors
    EfiACPIReclaimMemory,       // 9  - ACPI tables (reclaimable after parsing)
    EfiACPIMemoryNVS,           // 10 - ACPI NVS memory (must preserve)
    EfiMemoryMappedIO,          // 11 - MMIO
    EfiMemoryMappedIOPortSpace, // 12 - MMIO port space
    EfiPalCode,                 // 13 - Reserved for firmware
    EfiPersistentMemory,        // 14 - Persistent memory (NVDIMM)
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

//=============================================================================
// Memory Descriptor (UEFI Spec 7.2)
// Returned by GetMemoryMap()
//=============================================================================

typedef struct {
    UINT32                Type;           // EFI_MEMORY_TYPE
    EFI_PHYSICAL_ADDRESS  PhysicalStart;  // Physical address of region
    EFI_VIRTUAL_ADDRESS   VirtualStart;   // Virtual address (usually 0 until SetVirtualAddressMap)
    UINT64                NumberOfPages;  // Size in 4KB pages
    UINT64                Attribute;      // Memory attributes
} EFI_MEMORY_DESCRIPTOR;

//=============================================================================
// Allocation Types (UEFI Spec 7.2)
//=============================================================================

typedef enum {
    AllocateAnyPages,     // Allocate any available pages
    AllocateMaxAddress,   // Allocate below specified address
    AllocateAddress,      // Allocate at specified address
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

//=============================================================================
// Boot Services Function Pointer Types
//=============================================================================

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_PAGES)(
    EFI_ALLOCATE_TYPE     Type,
    EFI_MEMORY_TYPE       MemoryType,
    UINTN                 Pages,
    EFI_PHYSICAL_ADDRESS *Memory
);

typedef EFI_STATUS (EFIAPI *EFI_FREE_PAGES)(
    EFI_PHYSICAL_ADDRESS Memory,
    UINTN                Pages
);

typedef EFI_STATUS (EFIAPI *EFI_GET_MEMORY_MAP)(
    UINTN                 *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN                 *MapKey,
    UINTN                 *DescriptorSize,
    UINT32                *DescriptorVersion
);

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL)(
    EFI_MEMORY_TYPE PoolType,
    UINTN           Size,
    VOID          **Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL)(
    VOID *Buffer
);

typedef EFI_STATUS (EFIAPI *EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE ImageHandle,
    UINTN      MapKey
);

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL)(
    EFI_GUID *Protocol,
    VOID     *Registration,
    VOID    **Interface
);

//=============================================================================
// EFI Boot Services Table (UEFI Spec 4.4)
// 
// This table contains function pointers for all boot-time services.
// The layout must exactly match the spec - offsets matter!
//
// We define unused functions as VOID* placeholders to maintain offsets.
//=============================================================================

struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;

    //-------------------------------------------------------------------------
    // Task Priority Services (UEFI Spec 7.1)
    //-------------------------------------------------------------------------
    VOID *RaiseTpl;                     // Not typically needed
    VOID *RestoreTpl;

    //-------------------------------------------------------------------------
    // Memory Services (UEFI Spec 7.2)
    //-------------------------------------------------------------------------
    EFI_ALLOCATE_PAGES   AllocatePages;
    EFI_FREE_PAGES       FreePages;
    EFI_GET_MEMORY_MAP   GetMemoryMap;
    EFI_ALLOCATE_POOL    AllocatePool;
    EFI_FREE_POOL        FreePool;

    //-------------------------------------------------------------------------
    // Event & Timer Services (UEFI Spec 7.1)
    //-------------------------------------------------------------------------
    VOID *CreateEvent;
    VOID *SetTimer;
    VOID *WaitForEvent;
    VOID *SignalEvent;
    VOID *CloseEvent;
    VOID *CheckEvent;

    //-------------------------------------------------------------------------
    // Protocol Handler Services (UEFI Spec 7.3)
    //-------------------------------------------------------------------------
    VOID *InstallProtocolInterface;
    VOID *ReinstallProtocolInterface;
    VOID *UninstallProtocolInterface;
    VOID *HandleProtocol;
    VOID *Reserved;
    VOID *RegisterProtocolNotify;
    VOID *LocateHandle;
    VOID *LocateDevicePath;
    VOID *InstallConfigurationTable;

    //-------------------------------------------------------------------------
    // Image Services (UEFI Spec 7.4)
    //-------------------------------------------------------------------------
    VOID *LoadImage;
    VOID *StartImage;
    VOID *Exit;
    VOID *UnloadImage;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;

    //-------------------------------------------------------------------------
    // Miscellaneous Services (UEFI Spec 7.5)
    //-------------------------------------------------------------------------
    VOID *GetNextMonotonicCount;
    VOID *Stall;
    VOID *SetWatchdogTimer;

    //-------------------------------------------------------------------------
    // Driver Support Services (UEFI Spec 7.3)
    //-------------------------------------------------------------------------
    VOID *ConnectController;
    VOID *DisconnectController;

    //-------------------------------------------------------------------------
    // Open/Close Protocol Services (UEFI Spec 7.3)
    //-------------------------------------------------------------------------
    VOID *OpenProtocol;
    VOID *CloseProtocol;
    VOID *OpenProtocolInformation;

    //-------------------------------------------------------------------------
    // Library Services (UEFI Spec 7.3)
    //-------------------------------------------------------------------------
    VOID *ProtocolsPerHandle;
    VOID *LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;

    VOID *InstallMultipleProtocolInterfaces;
    VOID *UninstallMultipleProtocolInterfaces;

    //-------------------------------------------------------------------------
    // CRC32 Services (UEFI Spec 7.5)
    //-------------------------------------------------------------------------
    VOID *CalculateCrc32;

    //-------------------------------------------------------------------------
    // Miscellaneous Services (UEFI Spec 7.5)
    //-------------------------------------------------------------------------
    VOID *CopyMem;
    VOID *SetMem;
    VOID *CreateEventEx;
};
