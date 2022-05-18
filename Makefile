CC=gcc
CCOPTS=--std=c89 -Wall -Wextra -Wpedantic -Werror
AR=ar

HEADERS=FAT.h\

OBJS=FAT.o\

LIBS=libfat.a

BINS=fat_test

.phony: clean all


all:	$(LIBS) $(BINS)

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

libfat.a: $(OBJS) $(HEADERS) 
	$(AR) -rcs $@ $^
	$(RM) $(OBJS)

fat_test:		main.c $(LIBS)
	$(CC) $(CCOPTS) -o $@ $^

clean:
	rm -rf *.o *~ $(LIBS) $(BINS)
