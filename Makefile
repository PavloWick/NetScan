CC       ?= gcc
PREFIX   ?= /usr/local
BINDIR   ?= $(PREFIX)/bin
DATADIR  ?= $(PREFIX)/share

CFLAGS  ?= -Wall -Wextra -O2
CPPFLAGS += -Iinclude -DNETSCAN_OUI_PATH=\"$(DATADIR)/netscan/manuf\"
LDLIBS  += -lpcap

SRC  = $(wildcard src/*.c)
OBJ  = $(SRC:src/%.c=build/%.o)
BIN  = build/netscan

.PHONY: all clean install uninstall run

all: $(BIN)

$(BIN): $(OBJ)
	@$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/%.o: src/%.c | build
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build:
	@mkdir -p build

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(DATADIR)/netscan
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/netscan
	install -m 644 data/manuf $(DESTDIR)$(DATADIR)/netscan/manuf

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/netscan
	rm -f $(DESTDIR)$(DATADIR)/netscan/manuf
	rmdir $(DESTDIR)$(DATADIR)/netscan 2>/dev/null || true

run: all
	sudo ./$(BIN)

clean:
	@rm -rf build netscan
