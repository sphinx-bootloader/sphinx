ARCH := x86_64

CC := clang
LD := ld
OBJCOPY := objcopy

TRIPLE := x86_64-unknown-linux-gnu
EFI_ARCH_SUFFIX := X64

EFI_DIR := external/gnu-efi
EFI_INC := $(EFI_DIR)/inc
EFI_LIB := $(EFI_DIR)/x86_64/lib
EFI_GNUEFI_LIB := $(EFI_DIR)/x86_64/gnuefi
EFI_CRT := $(EFI_GNUEFI_LIB)/crt0-efi-x86_64.o
EFI_LDS := $(EFI_DIR)/gnuefi/elf_x86_64_efi.lds

TARGET := build/BOOT$(EFI_ARCH_SUFFIX).EFI
ELF_TARGET := build/BOOT$(EFI_ARCH_SUFFIX).so

SRCS := $(shell find src -name '*.c' 2>/dev/null | LC_ALL=C sort)
OBJS := $(patsubst src/%.c,obj/$(ARCH)/%.o,$(SRCS))

CFLAGS := --target=$(TRIPLE) \
    -ffreestanding \
    -fpic \
    -fshort-wchar \
    -fno-stack-protector \
    -fno-builtin \
    -mno-red-zone \
    -DGNU_EFI_USE_MS_ABI \
    -Wall -Wextra \
    -O2 -g \
    -Iinclude \
    -I$(EFI_INC)

.PHONY: all clean run

all: $(TARGET) edk2-ovmf-bins

$(ELF_TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) -nostdlib -znocombreloc -z defs \
	    -T $(EFI_LDS) -shared -Bsymbolic \
	    -L$(EFI_LIB) -L$(EFI_GNUEFI_LIB) \
	    $(EFI_CRT) -o $@ $(OBJS) -lgnuefi -lefi

$(TARGET): $(ELF_TARGET)
	$(OBJCOPY) \
	    -j .text \
	    -j .sdata \
	    -j .data \
	    -j .rodata \
	    -j .dynamic \
	    -j .dynsym \
	    -j .rel \
	    -j .rela \
	    -j '.rel.*' \
	    -j '.rela.*' \
	    -j .reloc \
	    --target=efi-app-x86_64 \
	    $< $@

obj/$(ARCH)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

-include $(OBJS:.o=.d)

run: all
	ARCH=$(ARCH) $(MAKE) -f run.mk run

clean:
	rm -rf obj build

.INTERMEDIATE: edk2-ovmf-bins.tar.gz

edk2-ovmf-bins.tar.gz:
	curl -fL -o $@ https://github.com/osdev0/edk2-ovmf-stable-bins/releases/latest/download/edk2-ovmf-bins.tar.gz

edk2-ovmf-bins: edk2-ovmf-bins.tar.gz
	rm -rf edk2-ovmf-bins
	gunzip < edk2-ovmf-bins.tar.gz | tar -xf -
