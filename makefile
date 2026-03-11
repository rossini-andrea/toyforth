# Append my set of CFLAGS
# Return type is enabled to avoid UB
# TCO is enabled for avoiding stack overflow in the state machine.
CFLAGS += -Werror=return-type -foptimize-sibling-calls -Wall -Wextra -O2

default: tf

all: tf test

tf.o: tf.c abstractions.h

test.o: test.c abstractions.h

abstractions.o: abstractions.c abstractions.h

%.o: %.c
	cc $(CFLAGS) -c -o $@ $<

tf: tf.o abstractions.o
	gcc $^ -o $@

test: test.o abstractions.o
	gcc $^ -o $@

clean:
	-rm *.o tf test

.PHONY: default all clean

