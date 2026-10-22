ARCH := x86_64

DISK := build/sphinx-disk.img
DISK_SIZE_MB := 64
VARS := build/ovmf-vars.fd

TARGET := build/BOOTX64.EFI
EFI_APP := BOOTX64.EFI

KERNEL_DIR := test_kernel
KERNEL_BIN := $(KERNEL_DIR)/build/kernel.elf
KERNEL_DEST := DiskLayout/boot/kernel

QEMU := qemu-system-x86_64
QEMU_ARGS := -machine q35 -cpu qemu64

FIRMWARE := edk2-ovmf-bins/ovmf-code-x86_64.fd
VARS_TEMPLATE := edk2-ovmf-bins/ovmf-vars-x86_64.fd

comma := ,

.PHONY: run disk kernel clean-disk

kernel:
	$(MAKE) -C $(KERNEL_DIR)
	mkdir -p $(dir $(KERNEL_DEST))
	cp "$(KERNEL_BIN)" "$(KERNEL_DEST)"

disk: kernel $(TARGET)
	@test -f "$(FIRMWARE)" || { echo "Missing firmware: $(FIRMWARE)"; exit 1; }
	@command -v mkfs.vfat >/dev/null || { echo "Install dosfstools"; exit 1; }
	@command -v mcopy >/dev/null || { echo "Install mtools"; exit 1; }
	mkdir -p build
	dd if=/dev/zero of="$(DISK)" bs=1M count=$(DISK_SIZE_MB) status=none
	mkfs.vfat "$(DISK)"
	test -d DiskLayout || { echo "Missing DiskLayout directory"; exit 1; }
	mcopy -s -o -i "$(DISK)" DiskLayout/* ::
	@if ! mdir -i "$(DISK)" ::/EFI/BOOT >/dev/null 2>&1; then \
	    mmd -i "$(DISK)" ::/EFI; \
	    mmd -i "$(DISK)" ::/EFI/BOOT; \
	fi
	mcopy -o -i "$(DISK)" "$(TARGET)" ::/EFI/BOOT/$(EFI_APP)

run: disk
	@test -f "$(VARS_TEMPLATE)" || { echo "Missing firmware variables: $(VARS_TEMPLATE)"; exit 1; }
	cp "$(VARS_TEMPLATE)" "$(VARS)"
	$(QEMU) $(QEMU_ARGS) \
	    -m 256M \
	    -drive if=pflash,format=raw,unit=0,readonly=on,file="$(FIRMWARE)" \
	    -drive if=pflash,format=raw,unit=1,file="$(VARS)" \
	    -drive if=none,id=esp,format=raw,file="$(DISK)" \
	    -device ich9-ahci,id=sata \
	    -device ide-hd,drive=esp,bus=sata.0,bootindex=1 \
	    -boot order=c \
	    -nic none

clean-disk:
	rm -f "$(DISK)" "$(VARS)"
