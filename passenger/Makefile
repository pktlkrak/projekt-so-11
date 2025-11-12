CC+=-I../commons/include

all: main

main: main.o signal_helpers.o ../commons/obj/ioman_client.o
	${CC} -o $@ $^

main.o: signal_helpers.h ../commons/include/ioman.h ../commons/include/messages.h main.c

.PHONY: clean
clean:
	rm main *.o
