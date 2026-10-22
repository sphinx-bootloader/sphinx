#include "efierr.h"
#include <efi.h>
#include <efilib.h>
#include <file.h>
#include <elf/elfloader.h>
#include <config.h>

/*
 * Convert CHAR8 to CHAR16, returns EFI_SUCCESS or EFI_OUT_OF_RESOURCES,
 * takes src as the CHAR8 string, and out as the (NULL) buffer of the output
 * string
 * out should be NULL, as CHAR8_to_CHAR16 already allocates it
 */
EFI_STATUS CHAR8_to_CHAR16(IN CHAR8* src, OUT CHAR16** out)
{
    /*
     * Get length of the ascii string
     */
    UINTN ascii_len = AsciiStrLen(src);

    /*
     * Allocate memory for the CHAR16 string
     */
    *out = AllocatePool((ascii_len + 1) * sizeof(CHAR16));
    if (!(*out))
    {
        return EFI_OUT_OF_RESOURCES;
    }

    /*
     * Copy data
     */
    for (UINTN i = 0; i < ascii_len; i++)
    {
        (*out)[i] = src[i];
    }

    (*out)[ascii_len] = L'\0';

    return EFI_SUCCESS;
}

void test_print(char* msg)
{
    Print(L"%a", msg);
}

/*
 * Entry point to sphinx bootloader
 */
EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    /*
     * Initialise the file manager
     */
    EFI_STATUS Status = InitialiseFiles(ImageHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to initialise filesystem: %r\r\n", Status);
        return Status;
    }

    /*
     * Open the configurations file
     */
    EFI_FILE_HANDLE ConfigFileHandle;
    Status = FileOpen(L"sphinx\\configurations.cfg", EFI_FILE_MODE_READ, 0,
        &ConfigFileHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to open sphinx\\configurations.cfg file: %r\r\n",
            Status);
        return Status;
    }

    /*
     * Get file size
     */
    INT64 file_size = FileSize(ConfigFileHandle);
    if (file_size == -1)
    {
        Print(L"Failed to get file size...");
        return EFI_NOT_FOUND;
    }

    /*
     * Allocate memory for buffer
     * One extra byte so the buffer can be NUL-terminated
     */
    CHAR8* config_file = (CHAR8*)AllocatePool((UINTN)file_size + 1);
    if (!config_file)
    {
        Print(L"Failed to allocate memory: %r\r\n", EFI_OUT_OF_RESOURCES);
        return EFI_OUT_OF_RESOURCES;
    }

    /*
     * Read contents
     */
    UINTN read_size = (UINTN)file_size;
    Status = FileRead(ConfigFileHandle, &read_size, config_file);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to read configurations file: %r\r\n", Status);
        return Status;
    }

    config_file[read_size] = '\0';

    /*
     * Initialise configurations parser
     */
    CHAR16* config_file_16 = NULL;
    Status = CHAR8_to_CHAR16(config_file, &config_file_16);
    FreePool(config_file);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to convert CHAR8 to CHAR16: %r\r\n", Status);
        return Status;
    }

    Status = ConfigFileParser_LoadConfig(config_file_16);
    FreePool(config_file_16);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to parse configurations file: %r\r\n", Status);
        return Status;
    }

    /*
     * No need for the config file handle anymore
     */
    FileClose(ConfigFileHandle);

    /*
     * Get path of kernel executable
     * KERNEL=<PATH>
     */
    CHAR16* name = ConfigFileParser_GetKernelPath();
    if (name == NULL)
    {
        Print(L"KERNEL is not set in configurations.cfg\r\n");
        return EFI_NOT_FOUND;
    }
    Print(L"Loading kernel at: %s\r\n", name);

    /*
     * Now open the executable file
     */
    EFI_FILE_HANDLE ExecutableFileHandle;

    Status = FileOpen(name, EFI_FILE_MODE_READ, 0, &ExecutableFileHandle);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to open kernel file: %r\r\n", Status);
        return Status;
    }

    /*
     * Get size, needed for LoadELF
     */
    INTN exec_size = FileSize(ExecutableFileHandle);
    if (exec_size == -1)
    {
        Print(L"Failed to get executable size\r\n");
        return EFI_NOT_FOUND;
    }

    /*
     * Read contents
     */
    void* exec_data = AllocatePool(exec_size);
    if (!exec_data)
    {
        Print(L"Failed to allocate memory: %r\r\n", EFI_OUT_OF_RESOURCES);
        return EFI_OUT_OF_RESOURCES;
    }

    Status = FileRead(ExecutableFileHandle, &exec_size, exec_data);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to read executable: %r\r\n", Status);
        return Status;
    }

    /*
     * Close it, it's been read
     */
    FileClose(ExecutableFileHandle);

    /*
     * Create the ELF image
     */
    elf_image image;
    Status = LoadELF(exec_data, exec_size, &image);
    if (EFI_ERROR(Status))
    {
        Print(L"Failed to load executable into memory: %r\r\n", Status);
        return Status;
    }

    /*
     * And finally, run the kernel
     */
    Print(L"[BOOT] ELF loaded successfully\r\n");
    Print(L"[BOOT] Entry virtual address: 0x%lx\r\n", image.entry);

    Status = RunELF(ImageHandle, &image, NULL);

    Print(L"[BOOT] RunELF returned: %r\r\n", Status);

    UnloadELF(&image);

    return Status;
}
