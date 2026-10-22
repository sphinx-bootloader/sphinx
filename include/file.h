#pragma once

#include <efi.h>
#include <efilib.h>

/*
 * Initialise file manager
 */
EFI_STATUS InitialiseFiles(EFI_HANDLE ImageHandle);

/*
 * Open a file
 * FileName: File to be opened
 * OpenMode: EFI_FILE_MODE_READ
 * Attributes: 0
 * FileHandle: File handle opened
 * Returns result status, EFI_SUCCESS if succeeded and EFI_INVALID_PARAMETER if not
 * EFI_INVALID_PARAMETER almost always means that a passed parameter was NULL
 */
EFI_STATUS FileOpen(IN CHAR16* FileName, IN UINT64 OpenMode, IN UINT64 Attributes, OUT EFI_FILE_HANDLE* FileHandle);

/*
 * Close a file
 * FileHandle: File to close
 * Returns result status, EFI_SUCCESS if succeeded and EFI_INVALID_PARAMETER if not
 * EFI_INVALID_PARAMETER means FileHandle was NULL
 * If EFI_INVALID_PARAMETER, can ignore, not as vital as FileOpen
 */
EFI_STATUS FileClose(IN EFI_FILE_HANDLE FileHandle);

/*
 * Read from a file into a buffer
 * FileHandle: File to read from
 * BufferSize (Before operation): How many bytes to read
 * BufferSize (After operation): How many bytes were read
 * Buffer: Where read data will be placed
 * Returns EFI_STATUS, EFI_INVALID_PARAMETER if any parameter was NULL
 */
EFI_STATUS FileRead(IN EFI_FILE_HANDLE FileHandle, IN OUT UINTN* BufferSize, OUT VOID* Buffer);

/*
 * Get file size
 * FileHandle: File to get it's size
 * Returns -1 if failed
 * Otherwise returns file size (in bytes)
 */
INTN FileSize(EFI_FILE_HANDLE FileHandle);
