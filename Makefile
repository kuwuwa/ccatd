
# WRAPPER := docker run --rm -it -w /tmp -v ${PWD}:/tmp gcc
CC := $(WRAPPER) gcc
CFLAGS := -std=c11 -g -static

ccatd: ccatd.c

wrapper:
	$(WRAPPER) sh

clean:
	rm -f ccatd $(patsubst %.c,%.o,$(SOURCES))

.PHONY: test clean
