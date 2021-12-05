%.o: %.s
	brew-none-elf-as $< -o $@
%.elf: %.o
	brew-none-elf-ld $< -o $@
%.bin: %.elf
	brew-none-elf-objcopy -O binary --strip-all $< $@

.PRECIOUS: %.o %.elf %.bin

all: no_asm.bin
