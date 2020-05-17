
CC := gcc
CFLAGS := -std=c11 -g -c -static -Wall -Werror
LDFLAGS :=

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

build: $(OBJS)
	$(CC) -o ccatd $(OBJS) $(LDFLAGS)
	ctags -R

test: build
	bash ./test.bash

clean:
	rm -f ccatd $(patsubst %.c,%.o,$(SRCS)) _*

b: build

c: clean

t: test

.PHONY: test clean
