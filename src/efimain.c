#include <efi.h>

/*
 * Entry point to sphinx bootloader
 */
EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    /*
     * Check that we got a system table
     */
    if (!SystemTable || !ImageHandle)
    {
        return EFI_INVALID_PARAMETER;
    }

    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Hello, World! from sphinx bootloader\r\n");

    /*
     * Return successfully, kernel finished execution
     */
    return EFI_SUCCESS;
}
