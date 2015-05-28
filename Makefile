PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man

CFLAGS += -O2 -Wall -Werror
LIBS = -lxbps
SRCS = $(shell find -type f -name '*.c')
OBJS = $(SRCS:.c=.o)

all: plist2json

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

plist2json: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $^ -o $@

install:
	install -d $(DESTDIR)/$(PREFIX)/bin
	install -m755 plist2json $(DESTDIR)/$(PREFIX)/bin
	install -d $(DESTDIR)/$(MANDIR)/man1
	install -m644 plist2json.1 $(DESTDIR)/$(MANDIR)/man1

clean:
	-rm -f $(OBJS) plist2json

.PHONY: all install clean
