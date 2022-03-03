.PRECIOUS: %.o %.elf %.s %.log %.trace %.sdmp %.bdmp

%.s: %.py
	python $^ > $@

%.o: %.s
	brew-none-elf-as $^ -o $@

%.elf: %.o
	brew-none-elf-ld $^ -o $@

%.log: %.elf
	brew-none-elf-run $^ | tee $@

%.trace: %.elf
	brew-none-elf-run --trace-insn $^ 2>&1 | tee $@

%.sdmp: %.o
	brew-none-elf-objdump --prefix-address --show-raw-ins -d $^ > $@

%.bdmp: %.elf
	brew-none-elf-objdump --prefix-address --show-raw-ins -d $^ > $@
