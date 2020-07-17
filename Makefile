
CC := gcc
CFLAGS := -std=c11 -g -c -static -Wall -Werror
LDFLAGS :=

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

build: $(OBJS)
	$(CC) -o ccatd $(OBJS) $(LDFLAGS)
	ctags -R

test: build
	APP=ccatd bash ./test.bash

clean:
	rm -rf ccatd $(patsubst %.c,%.o,$(SRCS)) _*

.PHONY: test clean
