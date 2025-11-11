all: test

test: test.o ../client.o
	${CC} -o $@ $^

test.o: test.c ../ioman.h

.PHONY: clean

clean:
	rm *.o test