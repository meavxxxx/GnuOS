#include <uefi.h>

static void efi_puts(EFI_SYSTEM_TABLE *system_table, const CHAR16 *text)
{
    if (!system_table || !system_table->ConOut || !system_table->ConOut->OutputString || !text) {
        return;
    }

    (void)system_table->ConOut->OutputString(system_table->ConOut, text);
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    static const CHAR16 banner[] = {
        'G', 'N', 'U', ' ', 'O', 'S', ':', ' ',
        'U', 'E', 'F', 'I', ' ', 's', 't', 'u', 'b', ' ',
        'l', 'o', 'a', 'd', 'e', 'r', ' ', 'r', 'e', 'a', 'c', 'h', 'e', 'd', '.',
        '\r', '\n', 0
    };
    static const CHAR16 todo[] = {
        'G', 'N', 'U', ' ', 'O', 'S', ':', ' ',
        'n', 'e', 'x', 't', ' ', 's', 't', 'e', 'p', ' ',
        '-', ' ', 'l', 'o', 'a', 'd', ' ', 'k', 'e', 'r', 'n', 'e', 'l', ' ',
        'a', 'n', 'd', ' ', 'b', 'u', 'i', 'l', 'd', ' ', 'm', 'u', 'l', 't', 'i', 'b', 'o', 'o', 't', ' ',
        'i', 'n', 'f', 'o', '.', '\r', '\n', 0
    };

    (void)image_handle;
    efi_puts(system_table, banner);
    efi_puts(system_table, todo);
    return EFI_SUCCESS;
}
