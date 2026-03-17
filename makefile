# Append my set of CFLAGS
# Return type is enabled to avoid UB
CFLAGS += -Werror=return-type -Wall -Wextra -O2
BUILD = build
SRC = src

SOURCES = tf.c test.c abstractions.c
OBJECTS = $(SOURCES:%.c=$(BUILD)/%.o)
DEPEND = $(OBJECTS:.o=.d)

default: tf

all: tf test

$(BUILD):
	mkdir $(BUILD)

$(BUILD)/%.o: $(SRC)/%.c $(BUILD)
	cc $(CFLAGS) -c -MP -MMD -o $@ $<

-include $(DEPEND)

tf: $(BUILD)/tf.o $(BUILD)/abstractions.o
	gcc $^ -o $@

test: $(BUILD)/test.o $(BUILD)/abstractions.o
	gcc $^ -o $@

clean:
	-rm -Rf build
	-rm tf test

.PHONY: default all clean

