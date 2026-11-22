CC = gcc
CFLAGS = -Wall -Wextra -std=c99
OBJS = main.o dir.o freelist.o io.o

fs: $(OBJS)
	$(CC) $(CFLAGS) -o fs $(OBJS)

%.o: %.c fs.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f fs $(OBJS)

.PHONY: clean
