CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -I../src

.PHONY: all clean test

all: test_set_dcf_message test_set_wwvb_message

test: test_set_dcf_message test_set_wwvb_message
	./test_set_dcf_message
	./test_set_wwvb_message

test_set_dcf_message: test_set_dcf_message.c ../src/dcf77_util.c ../src/dcf77_util.h
	$(CC) $(CFLAGS) -o $@ test_set_dcf_message.c ../src/dcf77_util.c

test_set_wwvb_message: test_set_wwvb_message.c ../src/wwvb_util.c ../src/wwvb_util.h ../src/radio_clock_pulse.h
	$(CC) $(CFLAGS) -o $@ test_set_wwvb_message.c ../src/wwvb_util.c

clean:
	rm -f test_set_dcf_message test_set_wwvb_message
