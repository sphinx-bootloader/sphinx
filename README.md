# sphinx bootloader

# Instructions to build the bootloader
Firstly, convert the parser.re file to C using re2c
```bash
re2c parser.re -o src/parser.c
```
Then, do `make` to compile the BOOTX64.EFI executable, you can find it in build/
You can also test the bootloader using `make run`
