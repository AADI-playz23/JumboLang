build:
	nasm -f elf64 src/bootstrap.asm -o bootstrap.o
	ld bootstrap.o -o jumbol
	rm bootstrap.o

run: build
	./jumbol