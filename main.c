/*-
 * Copyright (c) 2015, 2020 Juan Romero Pardines <xtraeme@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>

#include <prop/proplib.h>

#include "defs.h"

static size_t indent_blanks = 0;
static bool compact_mode = false;

static void
usage(const char *cmd)
{
	fprintf(stderr, "Usage: %s [-c] [ <file.plist> | - ]\n", cmd);
	exit(EXIT_FAILURE);
}

static char *
string_read(int fd)
{
	char *str, *ptr, *tptr;

	if ((str = malloc(BUFSIZ)) == NULL) {
		perror("malloc");
	}

	for (ptr = str, tptr = &str[BUFSIZ];;) {
		ssize_t	len;

		if ((len = read(fd, ptr, (tptr - ptr) - 1)) == -1) {
			perror("read");
		} else if (len == 0) {
			break;
		}

		ptr += len;
		if (ptr == tptr - 1) {
			char 	*nstr;

			tptr += BUFSIZ;
			if ((nstr = realloc(str, tptr - str)) == NULL) {
				perror("realloc");
			}
			if (nstr != str) {
				tptr = &nstr[tptr - str];
				ptr = &nstr[ptr - str];
				str = nstr;
			}
		}
	}

	*ptr = '\0';
	return str;
}

static void
indent(void)
{
	if (compact_mode || indent_blanks >= SIZE_MAX)
		return;

	for (size_t i = 0; i < indent_blanks; i++)
		printf(" ");
}

static void
parse(prop_object_t obj, prop_object_t parent)
{
	prop_type_t type;

	type = prop_object_type(obj);
	switch (type) {
	case PROP_TYPE_BOOL:
		printf("%s", (prop_bool_true(obj) ? "true" : "false"));
		break;
	case PROP_TYPE_NUMBER:
		if (prop_number_unsigned(obj)) {
			printf("%ju", prop_number_unsigned_integer_value(obj));
		} else {
			printf("%jd", prop_number_integer_value(obj));
		}
		break;
	case PROP_TYPE_STRING:
		{
			size_t len = prop_string_size(obj);
			char *str = prop_string_cstring(obj);
			const char *strconst = prop_string_cstring_nocopy(obj);
			size_t c = 0;

			for (size_t i = 0; i < len; i++) {
				if (iscntrl(strconst[i])) {
					continue;
				}
				str[c++] = strconst[i];
			}
			str[c] = '\0';
			printf("\"%s\"", str);
			free(str);
		}
		break;
	case PROP_TYPE_DATA:
		{
			size_t len;
			printf("\"%s\"", base64_encode(prop_data_data_nocopy(obj), prop_data_size(obj), &len));
		}
		break;
	case PROP_TYPE_ARRAY:
		printf("[");
		if (!compact_mode) {
			printf("\n");
		}
		indent_blanks += 2;
		indent();
		{
			unsigned int i, len;

			len = prop_array_count(obj);
			for (i = 0; i < len; i++) {
				prop_object_t o = prop_array_get(obj, i);
				if (prop_object_type(o) == PROP_TYPE_DICTIONARY) {
					parse(o, prop_dictionary_get_keysym(parent, obj));
				} else {
					parse(o, NULL);
				}
				if (i+1 < len) {
					printf(",");
					if (!compact_mode) {
						printf("\n");
					}
					indent();
				}
			}
		}
		if (!compact_mode) {
			printf("\n");
		}
		indent_blanks -= 2;
		indent();
		printf("]");
		break;
	case PROP_TYPE_DICTIONARY:
		printf("{");
		if (!compact_mode) {
			printf("\n");
		}
		indent_blanks += 2;
		indent();
		{
			prop_array_t allkeys;
			unsigned int i, len;

			allkeys = prop_dictionary_all_keys(obj);
			len = prop_array_count(allkeys);
			for (i = 0; i < len; i++) {
				prop_object_t keysym;
				prop_array_t array;
				const char *key;

				keysym = prop_array_get(allkeys, i);
				array = prop_dictionary_get_keysym(obj, keysym);
				key = prop_dictionary_keysym_cstring_nocopy(keysym);
				printf("\"%s\":", key);
				if (!compact_mode) {
					printf(" ");
				}
				parse(array, obj);
				if (i+1 < len) {
					printf(",");
					if (!compact_mode) {
						printf("\n");
					}
					indent();
				}
			}
			prop_object_release(allkeys);
		}
		if (!compact_mode) {
			printf("\n");
		}
		indent_blanks -= 2;
		indent();
		printf("}");
		break;
	case PROP_TYPE_DICT_KEYSYM:
		{
			const char *key;

			key = prop_dictionary_keysym_cstring_nocopy(obj);
			parse(prop_dictionary_get(parent, key), NULL);
		}
		break;
	case PROP_TYPE_UNKNOWN:
	default:
		exit(EXIT_FAILURE);
	}
}

int
main(int argc, char **argv)
{
	prop_object_t obj;

	if (argc > 2) {
		usage(argv[0]);
	}
	if (argc == 1 || strcmp(argv[1], "-") == 0) {
		char	*str;

		str = string_read(STDIN_FILENO);
		if ((obj = prop_array_internalize(str)) == NULL &&
		    (obj = prop_dictionary_internalize(str)) == NULL) {
			perror("ERROR: invalid plist input");
		}
		free(str);
	} else {
		const char	*f;

		f = argv[1];
		if ((obj = prop_array_internalize_from_file(f)) == NULL &&
		    (obj = prop_dictionary_internalize_from_file(f)) == NULL) {
			perror("ERROR: invalid plist file");
		}
	}

	if (getenv("COMPACT_MODE")) {
		compact_mode = true;
	}
	parse(obj, NULL);
	printf("\n");

	exit(EXIT_SUCCESS);
}
