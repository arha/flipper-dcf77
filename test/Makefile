CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -I..

.PHONY: all clean test

all: test_set_dcf_message

test: test_set_dcf_message
	./test_set_dcf_message

test_set_dcf_message: test_set_dcf_message.c ../dcf77_util.c ../dcf77_util.h
	$(CC) $(CFLAGS) -o $@ test_set_dcf_message.c ../dcf77_util.c

clean:
	rm -f test_set_dcf_message
