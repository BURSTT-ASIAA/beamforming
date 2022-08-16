all: testasm testasm2

testasm: testasm.o asmfunc.o
	gcc -o testasm testasm.o asmfunc.o

testasm2: testasm2.o asmfunc.o
	gcc -o testasm2 testasm2.o asmfunc.o

asmfunc.o: asmfunc.asm
	nasm -f elf64 asmfunc.asm
#	nasm -f elf64 -g -F dwarf asmfunc.asm

eatsyscall: eatsyscall.o
	ld -m elf_i386 -o eatsyscall eatsyscall.o

eatsyscall.o: eatsyscall.asm
	nasm -f elf -g -F dwarf eatsyscall.asm
#	nasm -f elf -g -F stabs eatsyscall.asm

printf: printf.o
	gcc -o printf printf.o

printf.o: printf.asm
	nasm -f elf64 -g -F dwarf printf.asm

clean:
	rm -f *.o

empty:
	rm -f eatsyscall printf testasm *.o

