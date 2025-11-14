CC+=-I../commons/include

all: main

main: main.o ../commons/obj/ioman_client.o
	${CC} -o $@ $^

main.o: ../commons/include/ioman.h ../commons/include/messages.h ../commons/include/global_config.h main.c

.PHONY: clean
clean:
	rm main *.o
