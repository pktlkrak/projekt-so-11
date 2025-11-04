all: ioman client.o make_test

ioman: main.o ioman.h
	${CC} -o ioman main.o

client.o: ioman_client.c  ioman.h
	${CC} -c -o client.o ioman_client.c

.PHONY: clean make_test
clean:
	rm -rf ioman *.o
	cd test && ${MAKE} clean

make_test:
	cd test && ${MAKE}