SCRIPT_NAME=elf
TEMPLATE_NAME=elf
OUTPUT_FORMAT="elf32-brew"
# Apparently if we only have one kind of endian-ness, we don't need either of these...
#BIG_OUTPUT_FORMAT="elf32-brew"
#LITTLE_OUTPUT_FORMAT="elf32-brew"
TEXT_START_ADDR=0x1000
MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"
ARCH=brew
EMBEDDED=yes
STACK_ADDR=0x400000
