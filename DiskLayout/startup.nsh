@echo -off
fs0:
cd \EFI\BOOT\

echo Starting Sphinx bootloader...
BOOTX64.EFI

if %lasterror% == 0 then
    echo Sphinx exited successfully.
else
    echo Sphinx failed. Last error: %lasterror%
endif
