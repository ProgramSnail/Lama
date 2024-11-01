FLAGS=-m32 -g2 -fstack-protector-all

all: src/cli.c src/parser.c src/interpreter.c src/utils.c src/types.c src/stack.c
	$(CC) $(FLAGS) -o byterun -Iinclude/ src/cli.c src/parser.c src/interpreter.c src/utils.c src/types.c src/stack.c ../runtime/runtime.a

clean:
	$(RM) *.a *.o *~ byterun
