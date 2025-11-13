CC+=-I../commons/include

all: main make_test

main: main.o signal_helpers.o ../commons/obj/ioman_client.o
	${CC} -o $@ $^

main.o: signal_helpers.h ../commons/include/ioman.h ../commons/include/messages.h main.c

.PHONY: clean make_test
clean:
	rm main *.o
	cd test && ${MAKE} clean

make_test:
	cd test && ${MAKE}