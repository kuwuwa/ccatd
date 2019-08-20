
CC := gcc
CFLAGS := -std=c11 -g -static -Wall -error
LDFLAGS :=

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

ccatd: $(OBJS)
	$(CC) -o ccatd $(OBJS) $(LDFLAGS)

test: ccatd
	sh ./test.sh

clean:
	rm -f ccatd $(patsubst %.c,%.o,$(SRCS)) _*

.PHONY: test clean
