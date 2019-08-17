
WRAPPER := docker run --rm -it -w /tmp -v ${PWD}:/tmp gcc
CC := $(WRAPPER) gcc
CFLAGS := -std=c11 -g -static -Wall -error
LDFLAGS :=

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

ccatd: $(OBJS)
	$(CC) -o ccatd $(OBJS) $(LDFLAGS)

wrapper:
	$(WRAPPER) sh

test: ccatd
	$(WRAPPER) sh ./test.sh

clean:
	rm -f ccatd $(patsubst %.c,%.o,$(SRCS))

.PHONY: test clean
