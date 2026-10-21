ARCH ?= x86_64

CC := clang
LD := lld-link

ifeq ($(ARCH),x86_64)
    TRIPLE := x86_64-unknown-windows
    MACHINE := x64
    ARCH_CFLAGS := -mno-red-zone
else ifeq ($(ARCH),aarch64)
    TRIPLE := aarch64-unknown-windows
    MACHINE := arm64
    ARCH_CFLAGS :=
else
    $(error unsupported ARCH '$(ARCH)', use x86_64 or aarch64)
endif

ARCH_UPPER := $(shell echo $(ARCH) | tr a-z A-Z)

TARGET := build/BOOT$(ARCH_UPPER).EFI
SRCS := $(shell find src -name '*.c' 2>/dev/null | LC_ALL=C sort)
OBJS := $(patsubst src/%.c,obj/$(ARCH)/%.obj,$(SRCS))

CFLAGS := --target=$(TRIPLE) \
    -ffreestanding \
    -fno-stack-protector \
    -fshort-wchar \
    -Wall -Wextra \
    -O2 -g \
    -Iinclude \
    $(ARCH_CFLAGS)

LDFLAGS := /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:$(MACHINE)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) /out:$@ $(OBJS)

obj/$(ARCH)/%.obj: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.obj=.d) -c $< -o $@

-include $(OBJS:.obj=.d)

run: all
	ARCH=$(ARCH) ./run.sh

clean:
	rm -rf obj build
