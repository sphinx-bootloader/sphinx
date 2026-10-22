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

#define KERNEL_BASE 0xFFFFFFFF80000000ULL
#define KERNEL_STACK_PAGES 16

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

    if (elfhdr->e_type != ET_EXEC)
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
 * gave us a physical allocation for the entire image, this finds the
 * segment that contains vaddr, works out how far into the segment it is,
 * and adds that offset to the image's physical base
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

    UINT64 image_start = KERNEL_BASE;
    UINT64 image_end = KERNEL_BASE;
    UINTN load_count = 0;

    image->entry = 0;
    image->mapping_count = 0;

    /*
     * Find the complete virtual address range of the kernel
     */
    for (UINTN i = 0; i < elfhdr->e_phnum; i++)
    {
        const Elf64_Phdr* phdr = &phdrs[i];

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0)
        {
            continue;
        }

        if (phdr->p_filesz > phdr->p_memsz ||
            phdr->p_offset > FileSize ||
            phdr->p_filesz > FileSize - phdr->p_offset ||
            phdr->p_vaddr < KERNEL_BASE ||
            phdr->p_memsz > UINT64_MAX - phdr->p_vaddr)
        {
            Print(L"ELF Load Error: Invalid load segment\r\n");
            return EFI_LOAD_ERROR;
        }

        if (phdr->p_align > 1 &&
            ((phdr->p_align & (phdr->p_align - 1)) != 0 ||
             (phdr->p_vaddr % phdr->p_align) !=
             (phdr->p_offset % phdr->p_align)))
        {
            Print(L"ELF Load Error: Invalid segment alignment\r\n");
            return EFI_LOAD_ERROR;
        }

        if (phdr->p_vaddr + phdr->p_memsz > image_end)
        {
            image_end = phdr->p_vaddr + phdr->p_memsz;
        }

        load_count++;
    }

    if (load_count == 0 || image_end <= image_start)
    {
        Print(L"ELF Load Error: No loadable segments\r\n");
        return EFI_LOAD_ERROR;
    }

    /*
     * Allocate memory for the complete kernel image and stack
     */
    UINT64 image_size = image_end - image_start;

    if (image_size > UINT64_MAX - (EFI_PAGE_SIZE - 1))
    {
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN image_pages = EFI_SIZE_TO_PAGES(image_size);

    if (image_pages > ((UINTN)-1) - KERNEL_STACK_PAGES)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    UINTN total_pages = image_pages + KERNEL_STACK_PAGES;
    EFI_PHYSICAL_ADDRESS paddr = 0;

    status = uefi_call_wrapper(BS->AllocatePages, 4,
        AllocateAnyPages, EfiLoaderData, total_pages, &paddr);
    if (EFI_ERROR(status))
    {
        Print(L"ELF Load Error: Failed to allocate kernel memory\r\n");
        return status;
    }

    /*
     * Clear the complete image so gaps between segments are zeroed
     */
    SetMem((void*)(UINTN)paddr, image_pages * EFI_PAGE_SIZE, 0);

    /*
     * Load each segment at its corresponding image offset
     */
    for (UINTN i = 0; i < elfhdr->e_phnum; i++)
    {
        const Elf64_Phdr* phdr = &phdrs[i];

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0)
        {
            continue;
        }

        CopyMem(
            (void*)(UINTN)(paddr + (phdr->p_vaddr - image_start)),
            (void*)(file + phdr->p_offset),
            phdr->p_filesz);

        /*
         * The image was cleared above, so BSS is already zeroed
         */
        image->mappings[image->mapping_count].vaddr = phdr->p_vaddr;
        image->mappings[image->mapping_count].paddr =
            paddr + (phdr->p_vaddr - image_start);
        image->mappings[image->mapping_count].memsz = phdr->p_memsz;

        image->mapping_count++;
    }

    /*
     * Store the entry point
     */
    image->entry = elfhdr->e_entry;

    /*
     * Store the complete allocation in the first mapping
     * so UnloadELF can free the image and stack together
     */
    image->mappings[0].vaddr = image_start;
    image->mappings[0].paddr = paddr;
    image->mappings[0].memsz = total_pages * EFI_PAGE_SIZE;

    /*
     * The allocation is contiguous, so only one mapping is needed
     */
    image->mapping_count = 1;

    return EFI_SUCCESS;
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

    if (image->mapping_count != 0)
    {
        load_mapping* m = &image->mappings[0];

        status = uefi_call_wrapper(BS->FreePages, 2,
            m->paddr, EFI_SIZE_TO_PAGES(m->memsz));
    }

    image->mapping_count = 0;
    image->entry = 0;

    return status;
}

typedef void (__attribute__((sysv_abi)) *kernel_entry_t)(void *);

/*
 * Exit boot services and jump to the image entry point
 */
EFI_STATUS RunELF(EFI_HANDLE ImageHandle, elf_image* image, void* argument)
{
    if (image == NULL || image->mapping_count == 0)
    {
        return EFI_INVALID_PARAMETER;
    }

    /*
     * The first mapping contains the complete kernel allocation
     */
    load_mapping* m = &image->mappings[0];

    UINT64 image_size = m->memsz -
        KERNEL_STACK_PAGES * EFI_PAGE_SIZE;

    UINT64 entry_offset = image->entry - KERNEL_BASE;

    if (image->entry < KERNEL_BASE || entry_offset >= image_size)
    {
        Print(L"ELF Run Error: Entry point out of bounds\r\n");
        return EFI_LOAD_ERROR;
    }

    UINT64 stack_top = KERNEL_BASE + m->memsz;

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

    /*
     * Jump to the kernel with the stack immediately after the image
     */
    kernel_entry_t kernel = (kernel_entry_t)(UINTN)image->entry;

    __asm__ __volatile__(
        "mov %0, %%rsp\n"
        "and $-16, %%rsp\n"
        "xor %%rbp, %%rbp\n"
        "mov %1, %%rdi\n"
        "jmp *%2\n"
        :
        : "r"(stack_top), "r"(argument), "r"(kernel)
        : "memory", "rdi");

    __builtin_unreachable();

    return EFI_SUCCESS;
}
