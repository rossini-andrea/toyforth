# Append my set of CFLAGS
# Return type is enabled to avoid UB
# TCO is enabled for avoiding stack overflow in the state machine.
CFLAGS += -Werror=return-type -foptimize-sibling-calls -Wall -O2

default: tf

tf.o: tf.c abstractions.h
	cc ${CFLAGS} -c -o $@ tf.c 

test.o: test.c abstractions.h
	cc ${CFLAGS} -c -o $@ test.c 

abstractions.o: abstractions.c abstractions.h
	cc ${CFLAGS} -c -o $@ abstractions.c 

tf: tf.o abstractions.o
	gcc $^ -o $@

test: test.o abstractions.o
	gcc $^ -o $@

clean:
	-rm *.o tf test

