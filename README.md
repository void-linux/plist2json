## WTF

plist2json converts NetBSD's portable property list files to
the JSON format in pretty (default) and compact mode.

- [portableproplib](https://github.com/xtraeme/portableproplib)
- [JSON](https://www.json.org/json-en.html)

## Dependencies

- A C99 compiler (clang, gcc, pcc, tcc)
- [GNU make](https://www.gnu.org/software/make)
- [portableproplib](https://github.com/xtraeme/portableproplib)

## Building and installing

Accepted arguments passed in to make(1):

- CFLAGS (set to `-O2 -Wall -Werror -Wextra -g -pipe -std=c99`)
- LDFLAGS (set to `-L$PREFIX/lib`)
- PREFIX (set to `/usr/local`)
- MANDIR  (set to `$PREFIX/share/man`)
- STATIC (for static builds)

```
$ make PREFIX=~/plist2json install clean
```

## Examples

Converting the xbps pkgdb plist file:

```
$ plist2json /var/db/xbps/pkgdb-0.38.plist
```

Converting the xbps repository index plist file, on the fly with `tar(1)` and `plist2json(1)`:

```
$ tar -xOf x86_64-musl-repodata index.plist|plist2json
```

Use JSON compact mode:

```
$ COMPACT_MODE=1 plist2json index.plist
```

## License

[BSD-2-Clause LICENSE](COPYING)
