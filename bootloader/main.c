// bootloader/main.c
// UEFI Bootloader using our custom headers
//
// This bootloader:
// 1. Gets framebuffer info via GOP
// 2. Finds the ACPI RSDP
// 3. Loads the kernel from disk
// 4. Gets the memory map
// 5. Exits boot services
// 6. Jumps to the kernel
#include "../efi/efi.h"
#include "../common/bootinfo.h"

// Our boot info structure (will be passed to kernel)
static struct BootInfo g_boot_info;

//=============================================================================
// Console Output Helpers
//=============================================================================

static void print_char(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out, char c) {
    CHAR16 buf[2] = { (CHAR16)c, 0 };
    uefi_call_wrapper(out->OutputString, 2, out, buf);
}

static void print(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out, const char *s) {
    while (*s) {
        if (*s == '\n') print_char(out, '\r');
        print_char(out, *s++);
    }
}

static void print_hex(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out, UINT64 val) {
    const char *hex = "0123456789ABCDEF";
    char buf[17];
    
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[16] = 0;
    
    print(out, "0x");
    print(out, buf);
}

static void print_dec(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out, UINT64 val) {
    char buf[21];
    int i = 20;
    buf[i] = 0;
    
    if (val == 0) {
        buf[--i] = '0';
    } else {
        while (val > 0) {
            buf[--i] = '0' + (val % 10);
            val /= 10;
        }
    }
    
    print(out, &buf[i]);
}

//=============================================================================
// Initialize Graphics (GOP)
//=============================================================================

static EFI_STATUS init_graphics(EFI_SYSTEM_TABLE *ST) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status;

    status = uefi_call_wrapper(ST->BootServices->LocateProtocol, 3,
                               &gop_guid, NULL, (VOID **)&gop);
    if (EFI_ERROR(status)) {
        print(ST->ConOut, "ERROR: Failed to locate GOP\n");
        return status;
    }

    // Store framebuffer info
    g_boot_info.framebuffer.base   = gop->Mode->FrameBufferBase;
    g_boot_info.framebuffer.width  = gop->Mode->Info->HorizontalResolution;
    g_boot_info.framebuffer.height = gop->Mode->Info->VerticalResolution;
    g_boot_info.framebuffer.pitch  = gop->Mode->Info->PixelsPerScanLine * 4;
    g_boot_info.framebuffer.bpp    = 32;

    print(ST->ConOut, "Framebuffer: ");
    print_dec(ST->ConOut, g_boot_info.framebuffer.width);
    print(ST->ConOut, "x");
    print_dec(ST->ConOut, g_boot_info.framebuffer.height);
    print(ST->ConOut, " @ ");
    print_hex(ST->ConOut, g_boot_info.framebuffer.base);
    print(ST->ConOut, "\n");

    return EFI_SUCCESS;
}

//=============================================================================
// Find ACPI RSDP
//=============================================================================

static void find_rsdp(EFI_SYSTEM_TABLE *ST) {
    EFI_GUID acpi2_guid = EFI_ACPI_20_TABLE_GUID;
    EFI_GUID acpi1_guid = EFI_ACPI_TABLE_GUID;

    // Search configuration tables for ACPI RSDP
    for (UINTN i = 0; i < ST->NumberOfTableEntries; i++) {
        EFI_CONFIGURATION_TABLE *table = &ST->ConfigurationTable[i];
        
        if (guid_equal(&table->VendorGuid, &acpi2_guid) ||
            guid_equal(&table->VendorGuid, &acpi1_guid)) {
            g_boot_info.rsdp = table->VendorTable;
            print(ST->ConOut, "RSDP found @ ");
            print_hex(ST->ConOut, (UINT64)g_boot_info.rsdp);
            print(ST->ConOut, "\n");
            return;
        }
    }
    
    print(ST->ConOut, "WARNING: RSDP not found\n");
    g_boot_info.rsdp = NULL;
}

//=============================================================================
// Get Memory Map
//=============================================================================

static EFI_STATUS get_memory_map(EFI_SYSTEM_TABLE *ST, UINTN *out_key) {
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key, desc_size;
    UINT32 desc_version;
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = ST->BootServices;

    // First call to get required size
    status = uefi_call_wrapper(BS->GetMemoryMap, 5,
                               &map_size, NULL, &map_key, &desc_size, &desc_version);
    
    // Allocate buffer (with extra space for changes)
    map_size += 2 * desc_size;
    status = uefi_call_wrapper(BS->AllocatePool, 3,
                               EfiLoaderData, map_size, (VOID **)&map);
    if (EFI_ERROR(status)) return status;

    // Get the actual memory map
    status = uefi_call_wrapper(BS->GetMemoryMap, 5,
                               &map_size, map, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status)) return status;

    // Convert to our format
    g_boot_info.memory_map_count = 0;
    UINT8 *ptr = (UINT8 *)map;
    UINT8 *end = ptr + map_size;

    while (ptr < end && g_boot_info.memory_map_count < MAX_MEMORY_MAP_ENTRIES) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *)ptr;
        struct MemoryMapEntry *entry = &g_boot_info.memory_map[g_boot_info.memory_map_count];

        entry->base   = desc->PhysicalStart;
        entry->length = desc->NumberOfPages * 4096;

        // Classify memory type
        switch (desc->Type) {
            case EfiConventionalMemory:
            case EfiBootServicesCode:
            case EfiBootServicesData:
                entry->type = MEMORY_TYPE_USABLE;
                break;
            case EfiACPIReclaimMemory:
            case EfiACPIMemoryNVS:
                entry->type = MEMORY_TYPE_ACPI;
                break;
            case EfiMemoryMappedIO:
            case EfiMemoryMappedIOPortSpace:
                entry->type = MEMORY_TYPE_MMIO;
                break;
            default:
                entry->type = MEMORY_TYPE_RESERVED;
        }

        g_boot_info.memory_map_count++;
        ptr += desc_size;
    }

    *out_key = map_key;
    
    print(ST->ConOut, "Memory map: ");
    print_dec(ST->ConOut, g_boot_info.memory_map_count);
    print(ST->ConOut, " entries\n");

    return EFI_SUCCESS;
}

//=============================================================================
// Load Kernel from Disk
//=============================================================================

static EFI_STATUS load_kernel(EFI_SYSTEM_TABLE *ST, VOID **kernel_addr) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_PROTOCOL *root, *file;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID info_guid = EFI_FILE_INFO_GUID;
    EFI_STATUS status;
    EFI_BOOT_SERVICES *BS = ST->BootServices;

    // Get file system protocol
    status = uefi_call_wrapper(BS->LocateProtocol, 3,
                               &fs_guid, NULL, (VOID **)&fs);
    if (EFI_ERROR(status)) {
        print(ST->ConOut, "ERROR: No filesystem found\n");
        return status;
    }

    // Open root directory
    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status)) {
        print(ST->ConOut, "ERROR: Failed to open volume\n");
        return status;
    }

    // Open kernel file
    CHAR16 kernel_path[] = u"\\EFI\\BOOT\\kernel.bin";
    status = uefi_call_wrapper(root->Open, 5,
                               root, &file, kernel_path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        print(ST->ConOut, "ERROR: Failed to open kernel.bin\n");
        return status;
    }

    // Get file size
    UINT8 info_buf[sizeof(EFI_FILE_INFO) + 100];
    UINTN info_size = sizeof(info_buf);
    status = uefi_call_wrapper(file->GetInfo, 4,
                               file, &info_guid, &info_size, info_buf);
    if (EFI_ERROR(status)) {
        print(ST->ConOut, "ERROR: Failed to get file info\n");
        return status;
    }

    EFI_FILE_INFO *info = (EFI_FILE_INFO *)info_buf;
    UINTN kernel_size = info->FileSize;

    // Allocate memory for kernel at 1MB
    UINTN pages = EFI_SIZE_TO_PAGES(kernel_size);
    EFI_PHYSICAL_ADDRESS addr = 0x100000;
    
    status = uefi_call_wrapper(BS->AllocatePages, 4,
                               AllocateAddress, EfiLoaderData, pages, &addr);
    if (EFI_ERROR(status)) {
        // Try anywhere
        status = uefi_call_wrapper(BS->AllocatePages, 4,
                                   AllocateAnyPages, EfiLoaderData, pages, &addr);
        if (EFI_ERROR(status)) {
            print(ST->ConOut, "ERROR: Failed to allocate memory for kernel\n");
            return status;
        }
    }

    // Read kernel
    status = uefi_call_wrapper(file->Read, 3, file, &kernel_size, (VOID *)addr);
    
    uefi_call_wrapper(file->Close, 1, file);
    uefi_call_wrapper(root->Close, 1, root);

    if (EFI_ERROR(status)) {
        print(ST->ConOut, "ERROR: Failed to read kernel\n");
        return status;
    }

    *kernel_addr = (VOID *)addr;
    
    print(ST->ConOut, "Kernel loaded @ ");
    print_hex(ST->ConOut, addr);
    print(ST->ConOut, " (");
    print_dec(ST->ConOut, kernel_size);
    print(ST->ConOut, " bytes)\n");

    return EFI_SUCCESS;
}

//=============================================================================
// Entry Point
// NOTE: Do NOT use EFIAPI here! gnu-efi's crt0 converts MS ABI to System V
// before calling efi_main. Only protocol function pointers need ms_abi.
//=============================================================================

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) {
    EFI_STATUS status;
    VOID *kernel_addr;
    UINTN map_key;

    // Clear screen and print banner
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    print(ST->ConOut, "=== MyOS Bootloader ===\n\n");

    // Initialize graphics
    status = init_graphics(ST);
    if (EFI_ERROR(status)) return status;

    // Find ACPI tables
    find_rsdp(ST);

    // Load kernel
    status = load_kernel(ST, &kernel_addr);
    if (EFI_ERROR(status)) return status;

    // Get memory map (must be done last, right before ExitBootServices)
    status = get_memory_map(ST, &map_key);
    if (EFI_ERROR(status)) return status;

    print(ST->ConOut, "\nExiting boot services...\n");

    // Exit boot services - after this, no more UEFI calls!
    status = uefi_call_wrapper(ST->BootServices->ExitBootServices, 2,
                               ImageHandle, map_key);
    if (EFI_ERROR(status)) {
        // Memory map may have changed, get it again
        status = get_memory_map(ST, &map_key);
        status = uefi_call_wrapper(ST->BootServices->ExitBootServices, 2,
                                   ImageHandle, map_key);
    }

    // Jump to kernel!
    typedef void (*KernelEntry)(struct BootInfo *);
    KernelEntry kernel_entry = (KernelEntry)kernel_addr;
    kernel_entry(&g_boot_info);

    // Should never reach here
    while (1) {
        __asm__ volatile("hlt");
    }
}
