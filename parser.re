#include <efi.h>
#include <efilib.h>
#include <parser.h>

/*
 * Copy a token's text into a fixed buffer, NUL-terminated
 */
static EFI_STATUS copy_tok(CHAR16* dst, UINTN dst_len, const token* tk)
{
    if (tk->len + 1 > dst_len)
    {
        return EFI_LOAD_ERROR;
    }

    CopyMem(dst, (void*)tk->start, tk->len * sizeof(CHAR16));
    dst[tk->len] = L'\0';

    return EFI_SUCCESS;
}

/*
 * Lex one token from the cursor. Comments and spaces are skipped,
 * newlines are returned as TK_EOL.
 */
static token lex(lexer* lx)
{
    const CHAR16* YYCURSOR = lx->cursor;
    const CHAR16* YYMARKER;
    const CHAR16* start;
    token tk = { 0 };

std:
    start = YYCURSOR;

    /*!re2c
        re2c:yyfill:enable = 0;
        re2c:define:YYCTYPE = "CHAR16";

        "#" [^\n\x00]*                           { goto std; }
        [ \t\r]+                                 { goto std; }
        "\n"                                     { tk.id = TK_EOL; goto done; }
        "="                                      { tk.id = TK_ASSIGN; goto done; }
        "0x" [0-9a-fA-F]+                        { tk.id = TK_INT_LIT; goto done; }
        [0-9]+                                   { tk.id = TK_INT_LIT; goto done; }
        "\"" [^"\n\x00]* "\""                    { tk.id = TK_STR_LIT; goto done; }
        [a-zA-Z_.] [a-zA-Z0-9_./\\]*             { tk.id = TK_IDNTFR; goto done; }
        "\x00"                                   { tk.id = TK_EOF; goto done; }
        *                                        { tk.id = TK_ERR; goto done; }
    */

done:
    tk.start = start;
    tk.len = YYCURSOR - start;
    lx->cursor = YYCURSOR;

    return tk;
}

/*
 * Read the next key=value line. Returns EFI_NOT_FOUND at end of input.
 */
EFI_STATUS config_next(lexer* lx, config_pair* out)
{
    token tk;

    do
    {
        tk = lex(lx);
    } while (tk.id == TK_EOL);

    if (tk.id == TK_EOF)
    {
        return EFI_NOT_FOUND;
    }

    if (tk.id != TK_IDNTFR)
    {
        return EFI_LOAD_ERROR;
    }

    if (copy_tok(out->key, CFG_KEY_MAX, &tk) != EFI_SUCCESS)
    {
        return EFI_LOAD_ERROR;
    }

    tk = lex(lx);
    if (tk.id != TK_ASSIGN)
    {
        return EFI_LOAD_ERROR;
    }

    tk = lex(lx);
    if (tk.id != TK_IDNTFR && tk.id != TK_INT_LIT && tk.id != TK_STR_LIT)
    {
        return EFI_LOAD_ERROR;
    }

    if (copy_tok(out->value, CFG_VAL_MAX, &tk) != EFI_SUCCESS)
    {
        return EFI_LOAD_ERROR;
    }

    tk = lex(lx);
    if (tk.id != TK_EOL && tk.id != TK_EOF)
    {
        return EFI_LOAD_ERROR;
    }

    return EFI_SUCCESS;
}
