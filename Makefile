EXECBIN  = httpserver
SOURCE   = httpserver.c hashtable.c
OBJECTS  = $(SOURCE:.c=.o)
FORMATS  = $(SOURCE:.c=.fmt)

CC       = clang
FORMAT   = clang-format
CFLAGS   = -Wall -Wextra -Werror -pedantic 
LDFLAGS  = -L. asgn4_helper_funcs.a

.PHONY: all clean format

all: $(EXECBIN)

$(EXECBIN): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o : %.c asgn2_helper_funcs.h protocol.h queue.h rwlock.h hashtable.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(EXECBIN) $(OBJECTS)

format: $(FORMATS)

%.fmt: %.c
	$(FORMAT) -i -style=file $<
	touch $@

