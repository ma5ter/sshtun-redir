CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra
LDFLAGS ?=

PREFIX  ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1
DESTDIR ?=

PROG    := sshtun-redir
SRC     := sshtun-redir.c
MAN     := sshtun-redir.1

.PHONY: all clean install uninstall

all: $(PROG)

$(PROG): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(PROG) *.o

install: $(PROG)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 0755 "$(PROG)" "$(DESTDIR)$(BINDIR)/$(PROG)"
	install -d "$(DESTDIR)$(MANDIR)"
	install -m 0644 "$(MAN)" "$(DESTDIR)$(MANDIR)/$(MAN)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(PROG)"
	rm -f "$(DESTDIR)$(MANDIR)/$(MAN)"
