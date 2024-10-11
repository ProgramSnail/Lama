FLAGS=-m32 -g2 -fstack-protector-all

all: parser.o
	$(CC) $(FLAGS) -o byterun parser.o ../runtime/runtime.a

parser.o: src/parser.c
	$(CC) $(FLAGS) -Iinclude/ -g -c src/parser.c

clean:
	$(RM) *.a *.o *~ byterun
