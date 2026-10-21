#include <efi.h>
#include <efilib.h>
#include <file.h>

/*
 * Entry point to sphinx bootloader
 */
EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    /*
     * Initialise the file manager
     */
    EFI_STATUS Status = InitialiseFiles(ImageHandle);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to initialise filesystem: %r\r\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}
