CC 	   := gcc
CFLAGS := -Wall -Wextra -g -Iinclude
LDLIBS  = -lpcap -lncurses

SRC := $(wildcard src/*.c)
OBJ := $(SRC: src/%.c=build/%.o)


netscan: $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build netscan
