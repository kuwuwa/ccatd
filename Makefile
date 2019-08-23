
CC := gcc
CFLAGS := -std=c11 -g -c -static -Wall -Werror
LDFLAGS :=

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

ccatd: $(OBJS)
	$(CC) -o ccatd $(OBJS) $(LDFLAGS)

test: ccatd
	bash ./test.bash

clean:
	rm -f ccatd $(patsubst %.c,%.o,$(SRCS)) _*

.PHONY: test clean
