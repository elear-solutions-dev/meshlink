/*
    dropin.c -- a set of drop-in replacements for libc functions
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "system.h"

#include "xalloc.h"

#ifndef HAVE_GET_CURRENT_DIR_NAME
/*
  Replacement for the GNU get_current_dir_name function:

  get_current_dir_name will malloc(3) an array big enough to hold the
  current directory name.  If the environment variable PWD is set, and
  its value is correct, then that value will be returned.
*/
char *get_current_dir_name(void) {
	size_t size;
	char *buf;
	char *r;

	/* Start with 100 bytes.  If this turns out to be insufficient to
	   contain the working directory, double the size.  */
	size = 100;
	buf = xmalloc(size);

	errno = 0;                                      /* Success */
	r = getcwd(buf, size);

	/* getcwd returns NULL and sets errno to ERANGE if the bufferspace
	   is insufficient to contain the entire working directory.  */
	while(r == NULL && errno == ERANGE) {
		free(buf);
		size <<= 1;                             /* double the size */
		buf = xmalloc(size);
		r = getcwd(buf, size);
	}

	return buf;
}
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **buf, const char *fmt, ...) {
	int result;
	va_list ap;
	va_start(ap, fmt);
	result = vasprintf(buf, fmt, ap);
	va_end(ap);
	return result;
}

int vasprintf(char **buf, const char *fmt, va_list ap) {
	int status;
	va_list aq;
	int len;

	len = 4096;
	*buf = xmalloc(len);

	va_copy(aq, ap);
	status = vsnprintf(*buf, len, fmt, aq);
	va_end(aq);

	if(status >= 0)
		*buf = xrealloc(*buf, status + 1);

	if(status > len - 1) {
		len = status;
		va_copy(aq, ap);
		status = vsnprintf(*buf, len, fmt, aq);
		va_end(aq);
	}

	return status;
}
#endif

#ifndef HAVE_GETTIMEOFDAY
int gettimeofday(struct timeval *tv, void *tz) {
#ifdef HAVE_MINGW
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	uint64_t lt = (uint64_t)ft.dwLowDateTime | ((uint64_t)ft.dwHighDateTime << 32);
	lt -= 116444736000000000ULL;
	tv->tv_sec = lt / 10000000;
	tv->tv_usec = (lt / 10) % 1000000;
#else
#warning No high resolution time source!
	tv->tv_sec = time(NULL);
	tv->tv_usec = 0;
#endif
	return 0;
}
#endif

#ifndef HAVE_USLEEP
int usleep(long long usec) {
	struct timeval tv = {usec / 1000000, (usec / 1000) % 1000};
	select(0, NULL, NULL, NULL, &tv);
	return 0;
}
#endif
