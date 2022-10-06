PREFIX ?= /usr/local
MANDIR ?= $(PREFIX)/share/man

PCFLAGS = -O2 -Wall -Werror -Wextra -g -pipe -std=c99
PLDFLAGS = -L$(PREFIX)/lib
LIBS = -lprop
SRCS = $(shell find -type f -name '*.c')
OBJS = $(SRCS:.c=.o)

ifdef STATIC
LIBS = -lz -lprop
CFLAGS += -static
LDFLAGS += -static
endif

all: plist2json

$(OBJS): %.o: %.c
	$(CC) $(PCFLAGS) $(CFLAGS) -c $< -o $@

plist2json: $(OBJS)
	$(CC) $(PCFLAGS) $(CFLAGS) $(PLDFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

install: plist2json
	install -d $(DESTDIR)/$(PREFIX)/bin
	install -m755 plist2json $(DESTDIR)/$(PREFIX)/bin
	install -d $(DESTDIR)/$(MANDIR)/man1
	install -m644 plist2json.1 $(DESTDIR)/$(MANDIR)/man1

clean:
	-rm -f $(OBJS) plist2json

.PHONY: all install clean
