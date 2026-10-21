#include <file.h>

EFI_FILE_HANDLE g_Volume;

/*
 * Get the volume the EFI application was loaded from
 */
EFI_FILE_HANDLE GetVolume(EFI_HANDLE ImageHandle)
{
    EFI_LOADED_IMAGE *LoadedImage = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *IOVolume = NULL;
    EFI_FILE_HANDLE Volume = NULL;

    EFI_STATUS Status;
    EFI_GUID LipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID FsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    Status = uefi_call_wrapper(
        BS->HandleProtocol, 3,
        ImageHandle, &LipGuid, (void **)&LoadedImage
    );

    if (EFI_ERROR(Status))
    {
        return NULL;
    }

    Status = uefi_call_wrapper(
        BS->HandleProtocol, 3,
        LoadedImage->DeviceHandle, &FsGuid, (void **)&IOVolume
    );

    if (EFI_ERROR(Status))
    {
        return NULL;
    }

    Status = uefi_call_wrapper(
        IOVolume->OpenVolume, 2, IOVolume, &Volume
    );

    if (EFI_ERROR(Status))
    {
        return NULL;
    }

    return Volume;
}

/*
 * Initialise file manager
 */
EFI_STATUS InitialiseFiles(EFI_HANDLE ImageHandle)
{
    g_Volume = GetVolume(ImageHandle);

    return g_Volume ? EFI_SUCCESS : EFI_NOT_FOUND;
}

/*
 * Open a file, takes file name, open mode and attributes as input, and returns
 * result (EFI_STATUS) and FileHandle
 */
EFI_STATUS FileOpen(
    IN CHAR16 *FileName,
    IN UINT64 OpenMode,
    IN UINT64 Attributes,
    OUT EFI_FILE_HANDLE *FileHandle
)
{
    if (!FileName || !FileHandle || !g_Volume)
        return EFI_INVALID_PARAMETER;

    return uefi_call_wrapper(
        g_Volume->Open, 5,
        g_Volume, FileHandle, FileName, OpenMode, Attributes
    );
}

/*
 * Close a file handle
 */
EFI_STATUS FileClose(IN EFI_FILE_HANDLE FileHandle)
{
    if (!FileHandle)
        return EFI_INVALID_PARAMETER;

    return uefi_call_wrapper(FileHandle->Close, 1, FileHandle);
}

/*
 * Read from a file, BufferSize is the amount of bytes to read, and after the
 * operation, will be the amount of bytes read
 */
EFI_STATUS FileRead(
    IN EFI_FILE_HANDLE FileHandle,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
)
{
    if (!FileHandle || !BufferSize || (!Buffer && *BufferSize != 0))
        return EFI_INVALID_PARAMETER;

    return uefi_call_wrapper(
        FileHandle->Read, 3,
        FileHandle, BufferSize, Buffer
    );
}

/*
 * Get file size, when it returns -1, this means operation failed
 */
INT64 FileSize(EFI_FILE_HANDLE FileHandle)
{
    if (!FileHandle)
        return -1;

    EFI_FILE_INFO *FileInfo = LibFileInfo(FileHandle);

    if (!FileInfo)
        return -1;

    INT64 Size = (INT64)FileInfo->FileSize;
    FreePool(FileInfo);

    return Size;
}
