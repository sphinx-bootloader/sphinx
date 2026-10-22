#include <efi.h>
#include <efilib.h>
#include <config.h>
#include <parser.h>

typedef struct _kvc
{
    CHAR16* key;
    CHAR16* value;
    struct _kvc* next;
} key_value_cache;

static key_value_cache* first = NULL;
static UINTN num_caches = 0;

/*
 * Allocate a heap copy of a NUL-terminated string
 */
static CHAR16* dup_str(const CHAR16* src)
{
    UINTN size = (StrLen(src) + 1) * sizeof(CHAR16);
    CHAR16* out = AllocatePool(size);

    if (out == NULL)
    {
        return NULL;
    }

    CopyMem(out, (void*)src, size);

    return out;
}

/*
 * Store a parsed pair on the cache list. Duplicate keys are kept,
 * and lookups return the last one in the file.
 */
static EFI_STATUS cache_add(const config_pair* pair)
{
    key_value_cache* node = AllocatePool(sizeof(*node));
    if (node == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    node->key = dup_str(pair->key);
    if (node->key == NULL)
    {
        FreePool(node);
        return EFI_OUT_OF_RESOURCES;
    }

    node->value = dup_str(pair->value);
    if (node->value == NULL)
    {
        FreePool(node->key);
        FreePool(node);
        return EFI_OUT_OF_RESOURCES;
    }

    node->next = first;
    first = node;
    num_caches++;

    return EFI_SUCCESS;
}

/*
 * Load configuration file
 * Parses the configuration and caches all keys and values
 */
EFI_STATUS ConfigFileParser_LoadConfig(CHAR16* config)
{
    lexer lx = { config };
    config_pair pair;
    EFI_STATUS status;

    for (;;)
    {
        status = config_next(&lx, &pair);

        if (status == EFI_NOT_FOUND)
        {
            return EFI_SUCCESS;
        }

        if (EFI_ERROR(status))
        {
            Print(L"Config Error: Malformed line\r\n");
            return status;
        }

        status = cache_add(&pair);
        if (EFI_ERROR(status))
        {
            return status;
        }
    }
}

/*
 * Get kernel path
 * Key: KERNEL
 */
CHAR16* ConfigFileParser_GetKernelPath(void)
{
    key_value_cache* node = first;

    while (node != NULL)
    {
        if (StrCmp(node->key, L"KERNEL") == 0)
        {
            return node->value;
        }

        node = node->next;
    }

    return NULL;
}
