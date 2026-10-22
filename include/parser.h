#pragma once

#include <efi.h>

#define TK_IDNTFR  0
#define TK_ASSIGN  1
#define TK_INT_LIT 2
#define TK_STR_LIT 3
#define TK_EOF     4
#define TK_ERR     5
#define TK_EOL 6

#define CFG_KEY_MAX 32
#define CFG_VAL_MAX 256

typedef struct
{
    int id;
    const CHAR16* start;
    UINTN len;
} token;

typedef struct
{
    const CHAR16* cursor;
} lexer;

typedef struct
{
    CHAR16 key[CFG_KEY_MAX];
    CHAR16 value[CFG_VAL_MAX];
} config_pair;

EFI_STATUS config_next(lexer* lx, config_pair* out);
