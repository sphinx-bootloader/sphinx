ARCH ?= x86_64

CC := clang
LD := lld-link

ifeq ($(ARCH),x86_64)
    TRIPLE := x86_64-unknown-windows
    MACHINE := x64
    ARCH_CFLAGS := -mno-red-zone -DEFI_PLATFORM=1
else ifeq ($(ARCH),aarch64)
    TRIPLE := aarch64-unknown-windows
    MACHINE := arm64
    ARCH_CFLAGS :=
else
    $(error unsupported ARCH '$(ARCH)', use x86_64 or aarch64)
endif

ifeq ($(ARCH),x86_64)
    EFI_ARCH_SUFFIX := X64
else ifeq ($(ARCH),aarch64)
    EFI_ARCH_SUFFIX := AA64
endif

TARGET := build/BOOT$(EFI_ARCH_SUFFIX).EFI
SRCS := $(shell find src -name '*.c' 2>/dev/null | LC_ALL=C sort)
OBJS := $(patsubst src/%.c,obj/$(ARCH)/%.obj,$(SRCS))

CFLAGS := --target=$(TRIPLE) \
    -ffreestanding \
    -fno-stack-protector \
    -fshort-wchar \
    -Wall -Wextra \
    -O2 -g \
    -Iinclude \
    -Iexternal/efi \
    $(ARCH_CFLAGS)

LDFLAGS := /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:$(MACHINE)

.PHONY: all clean run

all: $(TARGET) edk2-ovmf-bins

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) /out:$@ $(OBJS)

obj/$(ARCH)/%.obj: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.obj=.d) -c $< -o $@

-include $(OBJS:.obj=.d)

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
