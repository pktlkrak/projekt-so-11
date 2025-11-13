CXX+=-I../commons/include

all: main

main: main.o ../commons/obj/ioman_client.o
	${CXX} -o $@ $^

main.o: ../commons/include/ioman.h ../commons/include/messages.h main.cpp

.PHONY: clean
clean:
	rm main *.o
