CC=gcc
CCOPTS=--std=c89 -Wall -Wextra -Wpedantic -Wc++-compat -Werror -D_POSIX_C_SOURCE=200112L -g
AR=ar

HEADERS=FAT.h\

OBJS=FAT.o\

LIBS=libfat.a

BINS=fat_test\
	directory_copy\
	directory_expand

.phony: clean all


all:	$(LIBS) $(BINS)

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

libfat.a: $(OBJS) $(HEADERS) 
	$(AR) -rcs $@ $^
	$(RM) $(OBJS)

fat_test:		main.c $(LIBS)
	$(CC) $(CCOPTS) -o $@ $^

directory_copy:		directory_copy.c $(LIBS)
	$(CC) $(CCOPTS) -o $@ $^

directory_expand:		directory_expand.c $(LIBS)
	$(CC) $(CCOPTS) -o $@ $^

clean:
	rm -rf *.o *~ $(LIBS) $(BINS)
