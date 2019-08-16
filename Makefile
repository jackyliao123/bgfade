CC=gcc
CFLAGS=-Wall
LDFLAGS=-lX11 -lXrender -lXrandr -lm

bgfade: main.o vector.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c stb_image.h help_text.h vector.h
	$(CC) -c -o $@ $< $(CFLAGS)

vector.o: vector.c vector.h
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm *.o bgfade
