FLAGS=-m32 -g2 -fstack-protector-all

all: src/cli.c src/parser.c src/interpreter.c src/utils.c src/types.c src/virt_stack.c
	$(CC) $(FLAGS) -o byterun -Iinclude/ src/utils.c src/parser.c src/virt_stack.c src/interpreter.c src/cli.c ../runtime/runtime.a

clean:
	$(RM) *.a *.o *~ byterun
