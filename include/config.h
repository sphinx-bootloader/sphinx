#pragma once

#include <efi.h>

/*
 * Load configuration file
 * Parses the configuration and caches all keys and values
 */
EFI_STATUS ConfigFileParser_LoadConfig(CHAR16* config);

/*
 * Get kernel path
 * Key: KERNEL
 */
CHAR16* ConfigFileParser_GetKernelPath();
