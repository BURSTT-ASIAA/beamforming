all: testasm testasm2

testasm: testasm.o asmfunc_mem.o
	gcc -o testasm_mem testasm.o asmfunc_mem.o

testasm2: testasm2.o asmfunc_mem.o
	gcc -o testasm2_mem testasm2.o asmfunc_mem.o

asmfunc_mem.o: asmfunc_mem.asm
	nasm -f elf64 asmfunc_mem.asm
#	nasm -f elf64 -g -F dwarf asmfunc_mem.asm

%.o: %.c
	gcc -fPIC -c $<

clean:
	rm -f *.o

empty:
	rm -f testasm_mem testasm2_mem *.o
