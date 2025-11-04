all: test

test: test.o ../client.o ../ioman.h
	${CC} -o test test.o ../client.o

.PHONY: clean

clean:
	rm *.o test