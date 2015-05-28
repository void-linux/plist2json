/*-
 * Copyright (c) 2015 Juan Romero Pardines.
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
#include <xbps.h>

#include "defs.h"

#define STRING_ALLOC_INCREMENTS	4096
#define INDENT_INITIAL_SPACES	2

static char	*indent_spaces = NULL;
static size_t	indent_current_spaces = 0;
static int	indent_level = 0;

static void
err_usage(const char *cmd)
{
	(void)fprintf(stderr, "Usage: %s [ <file.plist> | - ]\n", cmd);
	exit(EXIT_FAILURE);
}

static void
err_invalid(const char *cmd, const char *file)
{
	if (file != NULL) {
		if (errno != 0) {
			fprintf(stderr, "%s", file);
			exit(EXIT_FAILURE);
		} else {
			(void)fprintf(stderr,
			    "%s: %s: Not a valid property list\n", cmd, file);
		}
	} else
		(void)fprintf(stderr, "%s: stdin: Not a valid property list\n",
		    cmd);
	exit(EXIT_FAILURE);
}

static char *
string_read(int fd)
{
	char *str, *ptr, *tptr;

	if ((str = malloc(STRING_ALLOC_INCREMENTS)) == NULL)
		perror("malloc");

	for (ptr = str, tptr = &str[STRING_ALLOC_INCREMENTS];;) {
		ssize_t	len;

		if ((len = read(fd, ptr, (tptr - ptr) - 1)) == -1)
			perror("read");
		else if (len == 0)
			break;

		ptr += len;
		if (ptr == tptr - 1) {
			char 	*nstr;

			tptr += STRING_ALLOC_INCREMENTS;
			if ((nstr = realloc(str, tptr - str)) == NULL)
				perror("realloc");
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
indent_init(void)
{
	if ((indent_spaces = malloc(INDENT_INITIAL_SPACES)) == NULL)
		perror("malloc");

	(void)memset(indent_spaces, ' ', INDENT_INITIAL_SPACES);
	indent_level = 0;
	indent_current_spaces = INDENT_INITIAL_SPACES;
}

static void
indent_write(int i)
{
	if (i > indent_current_spaces) {
		while ((indent_current_spaces *= 1) < i)
			;
		if ((indent_spaces = realloc(indent_spaces,
		    indent_current_spaces)) == NULL)
			perror("realloc");
		(void)memset(indent_spaces, ' ', indent_current_spaces);
	}
	(void)printf("\n");
	if (fwrite(indent_spaces, 1, i, stdout) != i)
		perror("fwrite");
}

static void
object_out(xbps_object_t obj, xbps_object_t parent)
{
	xbps_type_t type;

	type = xbps_object_type(obj);
	switch (type) {
	case XBPS_TYPE_BOOL:
		(void)printf("%s", (xbps_bool_true(obj) ? "true" : "false"));
		break;
	case XBPS_TYPE_NUMBER:
		if (xbps_number_unsigned(obj)) {
			uint64_t i = xbps_number_unsigned_integer_value(obj);

			(void)printf("%ju", i);
		} else {
			int64_t i = xbps_number_integer_value(obj);

			(void)printf("%jd", i);
		}
		break;
	case XBPS_TYPE_STRING:
		(void)printf("\"%s\"", xbps_string_cstring_nocopy(obj));
		break;
	case XBPS_TYPE_DATA:
		{
			size_t len;
			(void)printf("\"%s\"", base64_encode(xbps_data_data_nocopy(obj), xbps_data_size(obj), &len));
		}
#if 0
		(void)printf("\"0x");
		{
			size_t		i, len;
			const uint8_t	*ptr;

			for (i = 0, len = xbps_data_size(obj),
			    ptr = xbps_data_data_nocopy(obj);
			    i < len; i++)
				(void)printf("%02x", ptr[i]);
		}
		(void)printf("\"");
#endif
		break;
	case XBPS_TYPE_ARRAY:
		(void)printf("[ ");
		{
			xbps_object_iterator_t	i;
			xbps_object_t		o;
			unsigned int		cnt, len;

			for (cnt = 0, len = xbps_array_count(obj),
			    i = xbps_array_iterator(obj);
			    (o = xbps_object_iterator_next(i)) != NULL;
			    cnt++) {
				object_out(o, NULL);
				if (cnt+1 < len)
					(void)printf(",");
				else
					(void)printf(" ");
			}
			xbps_object_iterator_release(i);
		}
		(void)printf("]");
		break;
	case XBPS_TYPE_DICTIONARY:
		(void)printf("{");
		indent_level += 1;
		{
			xbps_object_iterator_t	i;
			xbps_object_t		o;
			unsigned int		cnt, len;

			for (cnt = 0, len = xbps_dictionary_count(obj),
			    i = xbps_dictionary_iterator(obj);
			    (o = xbps_object_iterator_next(i)) != NULL;
			    cnt++) {
				object_out(o, obj);
				if (cnt+1 < len)
					(void)printf(",");
			}
			xbps_object_iterator_release(i);
		}
		indent_level -= 1;
		indent_write(indent_level);
		(void)printf("}");
		break;
	case XBPS_TYPE_DICT_KEYSYM:
		indent_write(indent_level);
		(void)printf("\"%s\": ",
		    xbps_dictionary_keysym_cstring_nocopy(obj));
		object_out(xbps_dictionary_get_keysym(parent, obj), NULL);
		break;
	case XBPS_TYPE_UNKNOWN:
		break;
	}
}

int
main(int argc, char **argv)
{
	xbps_object_t obj = NULL;

	if (argc > 2)
		err_usage(argv[0]);

	if (argc == 1 || strcmp(argv[1], "-") == 0) {
		char	*str;

		str = string_read(STDIN_FILENO);
		if ((obj = xbps_array_internalize(str)) == NULL &&
		    (obj = xbps_dictionary_internalize(str)) == NULL)
			err_invalid(argv[0], NULL);
		free(str);
	} else {
		const char	*f;

		f = argv[1];
		if ((obj = xbps_array_internalize_from_file(f)) == NULL &&
		    (obj = xbps_dictionary_internalize_from_file(f)) == NULL)
			err_invalid(argv[0], f);
	}

	indent_init();
	object_out(obj, NULL);
	(void)printf("\n");
	free(obj);

	exit(EXIT_SUCCESS);
}
