CC+=-I../commons/include

all: main

main: main.o ../commons/obj/ioman_client.o
	${CC} -o $@ $^

main.o: ../commons/include/ioman.h ../commons/include/messages.h main.c

.PHONY: clean
clean:
	rm main *.o
