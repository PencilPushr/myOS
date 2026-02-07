// Minimal test bootloader
#include "../efi/efi.h"

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) {
    (void)ImageHandle;
    
    // Try the simplest possible call - OutputString
    CHAR16 msg[] = u"Hello from custom headers!\r\n";
    
    // Direct call through function pointer with ms_abi attribute
    ST->ConOut->OutputString(ST->ConOut, msg);
    
    // Draw to framebuffer to prove we're running
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    
    EFI_STATUS status = ST->BootServices->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);
    
    if (!EFI_ERROR(status) && gop && gop->Mode) {
        UINT32 *fb = (UINT32 *)gop->Mode->FrameBufferBase;
        UINT32 ppsl = gop->Mode->Info->PixelsPerScanLine;
        
        // Draw green rectangle
        for (UINT32 y = 100; y < 300; y++) {
            for (UINT32 x = 100; x < 400; x++) {
                fb[y * ppsl + x] = 0x0000FF00;
            }
        }
    }
    
    while (1) __asm__ volatile("hlt");
    return EFI_SUCCESS;
}
