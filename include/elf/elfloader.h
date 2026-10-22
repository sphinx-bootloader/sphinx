#pragma once

#include <efi.h>

#define MAX_LOAD_MAPPINGS 16

typedef struct
{
    UINT64 vaddr;
    EFI_PHYSICAL_ADDRESS paddr;
    UINT64 memsz;
} load_mapping;

typedef struct
{
    UINT64 entry;
    load_mapping mappings[MAX_LOAD_MAPPINGS];
    UINTN mapping_count;
} elf_image;

/*
 * Load ELF file
 * FileData is the ELF file
 * FileSize is the file size of the ELF file
 * image is the result parsed ELF
 */
EFI_STATUS LoadELF(void *FileData, UINTN FileSize, elf_image *image);

/*
 * Unload ELF and free all memory from image
 */
EFI_STATUS UnloadELF(elf_image* image);

/*
 * Exit boot services and jump to the image entry point
 */
EFI_STATUS RunELF(EFI_HANDLE ImageHandle, elf_image* image);
