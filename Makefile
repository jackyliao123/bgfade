CC=gcc
CFLAGS=-Wall
LDFLAGS=-lX11 -lXrender -lXrandr -lm

TARGET=bgfade
OBJ=vector.o main.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean

clean:
	rm *.o bgfade
