#include <elf/elf.h>
#include <elf/elfloader.h>
#include <efi.h>
#include <efilib.h>

/*
 * Compare data at ptr a to ptr b, length s
 */
int memcmp(const void* a, const void* b, size_t s)
{
    const uint8_t* pa = a;
    const uint8_t* pb = b;

    for (size_t i = 0; i < s; i++)
    {
        if (pa[i] != pb[i])
        {
            return (int)pa[i] - (int)pb[i];
        }
    }

    return 0;
}

/*
 * Check ELF file header
 */
static EFI_STATUS check_file(const Elf64_Ehdr* elfhdr)
{
    if (memcmp(elfhdr->e_ident, ELFMAG, SELFMAG) != 0)
    {
        Print(L"ELF Load Error: Invalid ELF magic number\r\n");
        return EFI_LOAD_ERROR;
    }

    if (elfhdr->e_ident[EI_CLASS] != ELFCLASS64)
    {
        Print(L"ELF Load Error: Expected 64-bit class\r\n");
        return EFI_UNSUPPORTED;
    }

    if (elfhdr->e_ident[EI_DATA] != ELFDATA2LSB)
    {
        Print(L"ELF Load Error: Expected little-endian ELF\r\n");
        return EFI_UNSUPPORTED;
    }

    if (elfhdr->e_ident[EI_VERSION] != EV_CURRENT ||
        elfhdr->e_version != EV_CURRENT)
    {
        Print(L"ELF Load Error: Unsupported ELF version\r\n");
        return EFI_LOAD_ERROR;
    }

    if (elfhdr->e_machine != EM_X86_64)
    {
        Print(L"ELF Load Error: Expected x86-64 machine type\r\n");
        return EFI_UNSUPPORTED;
    }

    if (elfhdr->e_type != ET_EXEC && elfhdr->e_type != ET_DYN)
    {
        Print(L"ELF Load Error: Unsupported ELF type\r\n");
        return EFI_UNSUPPORTED;
    }

    return EFI_SUCCESS;
}

/*
 * Translate an ELF virtual address into a pointer to where that data
 * actually sits in memory
 *
 * The ELF file says where each segment should live (p_vaddr), but UEFI
 * gave us whatever free pages it had, so each segment is at a different
 * physical address (paddr), this finds the segment that contains vaddr,
 * works out how far into the segment it is, and adds that offset to the
 * segment's physical base
 *
 * size is how many bytes the caller needs starting at vaddr The whole
 * range must fit inside the segment, otherwise we would return a pointer
 * that runs past the end of it
 *
 * Returns NULL if no segment contains the range
 */
static uint8_t* resolve_vaddr(elf_image* image, UINT64 vaddr, UINT64 size)
{
    for (UINTN i = 0; i < image->mapping_count; i++)
    {
        load_mapping* m = &image->mappings[i];

        if (vaddr >= m->vaddr &&
            size <= m->memsz &&
            vaddr - m->vaddr <= m->memsz - size)
        {
            return (uint8_t*)(UINTN)(m->paddr + (vaddr - m->vaddr));
        }
    }

    return NULL;
}

/*
 * Fix up pointers inside the loaded image
 *
 * The compiler doesn't know where the program will be loaded, so any
 * absolute address stored in the data (for example a pointer in a global
 * variable) is written as if the program lived at its link-time address
 * Those values are wrong for the copy we made, so the linker lists them in
 * a relocation table and we rewrite each one
 *
 * The steps are:
 *   1. Find the PT_DYNAMIC segment, it holds tags that describe the
 *      tables, including where the relocation table is
 *   2. Read DT_RELA (table address), DT_RELASZ (total size) and
 *      DT_RELAENT (size of one entry) from those tags
 *   3. For each entry, R_X86_64_RELATIVE means "store a value at r_offset"
 *      The value is r_addend, the link-time address of the target, writing
 *      it into the loaded copy makes the pointer valid only if that address
 *      is mapped, which is why RunELF needs paging or identity mapping
 *
 * If the image has no PT_DYNAMIC or no relocation table, there is nothing
 * to fix and we return success
 */
static EFI_STATUS apply_relocations(elf_image* image,
    const Elf64_Phdr* phdrs, UINTN phnum)
{
    const Elf64_Dyn* dyn = NULL;
    UINTN dyn_count = 0;
    UINT64 rela_vaddr = 0;
    UINT64 rela_size = 0;
    UINT64 rela_ent = sizeof(Elf64_Rela);

    for (UINTN i = 0; i < phnum; i++)
    {
        if (phdrs[i].p_type == PT_DYNAMIC)
        {
            dyn = (const Elf64_Dyn*)resolve_vaddr(image,
                phdrs[i].p_vaddr, phdrs[i].p_memsz);
            dyn_count = phdrs[i].p_memsz / sizeof(Elf64_Dyn);
            break;
        }
    }

    if (dyn == NULL)
    {
        return EFI_SUCCESS;
    }

    for (UINTN i = 0; i < dyn_count && dyn[i].d_tag != DT_NULL; i++)
    {
        switch (dyn[i].d_tag)
        {
        case DT_RELA:
            rela_vaddr = dyn[i].d_un.d_ptr;
            break;
        case DT_RELASZ:
            rela_size = dyn[i].d_un.d_val;
            break;
        case DT_RELAENT:
            rela_ent = dyn[i].d_un.d_val;
            break;
        }
    }

    if (rela_vaddr == 0)
    {
        return EFI_SUCCESS;
    }

    if (rela_ent != sizeof(Elf64_Rela))
    {
        Print(L"ELF Load Error: Unexpected relocation entry size\r\n");
        return EFI_LOAD_ERROR;
    }

    const Elf64_Rela* rela =
        (const Elf64_Rela*)resolve_vaddr(image, rela_vaddr, rela_size);
    if (rela == NULL)
    {
        Print(L"ELF Load Error: Relocation table out of bounds\r\n");
        return EFI_LOAD_ERROR;
    }

    for (UINT64 i = 0; i < rela_size / sizeof(Elf64_Rela); i++)
    {
        if (ELF64_R_TYPE(rela[i].r_info) != R_X86_64_RELATIVE)
        {
            Print(L"ELF Load Error: Unsupported relocation type\r\n");
            return EFI_UNSUPPORTED;
        }

        UINT64* target = (UINT64*)resolve_vaddr(image,
            rela[i].r_offset, sizeof(UINT64));
        if (target == NULL)
        {
            Print(L"ELF Load Error: Relocation target out of bounds\r\n");
            return EFI_LOAD_ERROR;
        }

        *target = rela[i].r_addend;
    }

    return EFI_SUCCESS;
}

/*
 * Load ELF file from FileData
 */
EFI_STATUS LoadELF(void* FileData, UINTN FileSize, elf_image* image)
{
    if (FileData == NULL || image == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (FileSize < sizeof(Elf64_Ehdr))
    {
        return EFI_LOAD_ERROR;
    }

    const Elf64_Ehdr* elfhdr = (const Elf64_Ehdr*)FileData;

    EFI_STATUS status = check_file(elfhdr);
    if (EFI_ERROR(status))
    {
        return status;
    }

    if (elfhdr->e_phnum == 0 || elfhdr->e_phentsize != sizeof(Elf64_Phdr))
    {
        Print(L"ELF Load Error: Invalid program header table\r\n");
        return EFI_LOAD_ERROR;
    }

    if (elfhdr->e_phoff > FileSize ||
        elfhdr->e_phnum > (FileSize - elfhdr->e_phoff) / elfhdr->e_phentsize)
    {
        Print(L"ELF Load Error: Program headers out of bounds\r\n");
        return EFI_LOAD_ERROR;
    }

    const uint8_t* file = FileData;
    const Elf64_Phdr* phdrs = (const Elf64_Phdr*)(file + elfhdr->e_phoff);

    image->entry = elfhdr->e_entry;
    image->mapping_count = 0;

    for (UINTN i = 0; i < elfhdr->e_phnum; i++)
    {
        const Elf64_Phdr* phdr = &phdrs[i];

        if (phdr->p_type != PT_LOAD)
        {
            continue;
        }

        if (phdr->p_filesz > phdr->p_memsz ||
            phdr->p_offset > FileSize ||
            phdr->p_filesz > FileSize - phdr->p_offset)
        {
            Print(L"ELF Load Error: Invalid load segment\r\n");
            status = EFI_LOAD_ERROR;
            goto fail;
        }

        if (phdr->p_memsz == 0)
        {
            continue;
        }

        if (phdr->p_vaddr & (EFI_PAGE_SIZE - 1))
        {
            Print(L"ELF Load Error: Unaligned load segment\r\n");
            status = EFI_LOAD_ERROR;
            goto fail;
        }

        if (image->mapping_count >= MAX_LOAD_MAPPINGS)
        {
            Print(L"ELF Load Error: Too many load segments\r\n");
            status = EFI_OUT_OF_RESOURCES;
            goto fail;
        }

        UINTN pages = EFI_SIZE_TO_PAGES(phdr->p_memsz);
        EFI_PHYSICAL_ADDRESS paddr = 0;

        status = uefi_call_wrapper(BS->AllocatePages, 4,
            AllocateAnyPages, EfiLoaderData, pages, &paddr);
        if (EFI_ERROR(status))
        {
            Print(L"ELF Load Error: Failed to allocate segment memory\r\n");
            goto fail;
        }

        CopyMem((void*)(UINTN)paddr, file + phdr->p_offset, phdr->p_filesz);
        SetMem((void*)(UINTN)(paddr + phdr->p_filesz),
            phdr->p_memsz - phdr->p_filesz, 0);

        image->mappings[image->mapping_count].vaddr = phdr->p_vaddr;
        image->mappings[image->mapping_count].paddr = paddr;
        image->mappings[image->mapping_count].memsz = phdr->p_memsz;
        image->mapping_count++;
    }

    status = apply_relocations(image, phdrs, elfhdr->e_phnum);
    if (EFI_ERROR(status))
    {
        goto fail;
    }

    return EFI_SUCCESS;

fail:
    UnloadELF(image);
    return status;
}

/*
 * Free all segment memory owned by an image
 */
EFI_STATUS UnloadELF(elf_image* image)
{
    if (image == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    EFI_STATUS status = EFI_SUCCESS;

    for (UINTN i = 0; i < image->mapping_count; i++)
    {
        load_mapping* m = &image->mappings[i];

        EFI_STATUS free_status =
            uefi_call_wrapper(BS->FreePages, 2,
                m->paddr, EFI_SIZE_TO_PAGES(m->memsz));

        if (EFI_ERROR(free_status) && status == EFI_SUCCESS)
        {
            status = free_status;
        }
    }

    image->mapping_count = 0;
    image->entry = 0;

    return status;
}

/*
 * Exit boot services and jump to the image entry point
 */
EFI_STATUS RunELF(EFI_HANDLE ImageHandle, elf_image* image)
{
    if (image == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    uint8_t* entry = resolve_vaddr(image, image->entry, 1);
    if (entry == NULL)
    {
        Print(L"ELF Run Error: Entry point out of bounds\r\n");
        return EFI_LOAD_ERROR;
    }

    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_version = 0;
    EFI_MEMORY_DESCRIPTOR* map = NULL;

    EFI_STATUS status = uefi_call_wrapper(BS->GetMemoryMap, 5,
        &map_size, NULL, &map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL)
    {
        return status;
    }

    for (;;)
    {
        map_size += 2 * desc_size;

        status = uefi_call_wrapper(BS->AllocatePool, 3,
            EfiLoaderData, map_size, (void**)&map);
        if (EFI_ERROR(status))
        {
            return status;
        }

        status = uefi_call_wrapper(BS->GetMemoryMap, 5,
            &map_size, map, &map_key, &desc_size, &desc_version);
        if (EFI_ERROR(status))
        {
            FreePool(map);
            return status;
        }

        status = uefi_call_wrapper(BS->ExitBootServices, 2,
            ImageHandle, map_key);
        if (!EFI_ERROR(status))
        {
            break;
        }

        FreePool(map);
    }

    ((void (*)(void))(UINTN)entry)();

    return EFI_SUCCESS;
}
