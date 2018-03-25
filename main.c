#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include "zstdwriter.h"

#define PROG "zstdwriter"

static void error(const char *func, const char *err[2])
{
    if (strcmp(func, err[0]) == 0)
	fprintf(stderr, PROG ": %s: %s\n", err[0], err[1]);
    else
	fprintf(stderr, PROG ": %s: %s: %s\n", func, err[0], err[1]);
}

int main(int argc, char **argv)
{
    if (isatty(1)) {
	fprintf(stderr, PROG ": compressed data cannot be written to a terminal\n");
	return 1;
    }
    if (isatty(0))
	fprintf(stderr, PROG ": reading input from a terminal\n");

    int compressionLevel = 3;
    if (argc > 1) {
	const char *opt = argv[1];
	if (opt[0] == '-' && opt[1] >= '0' && opt[1] <= '9')
	    compressionLevel = atoi(&opt[1]);
    }

    struct zstdwriter *zw;
    const char *err[2];
    if (lseek(1, 0, SEEK_CUR) == -1)
	zw = zstdwriter_fdopen(1, compressionLevel, err);
    else {
	ZSTD_compressionParameters cParams = ZSTD_getCParams(compressionLevel, 0, 0);
	zw = zstdwriter_fdopen_ex(1, cParams, true, true, err);
    }
    if (!zw)
	return error("zstdwriter_fdopen", err), 1;

    char buf[512<<10];
    while (1) {
	size_t size = 1 + rand() % sizeof buf;
	size = fread(buf, 1, size, stdin);
	if (size == 0)
	    break;
	if (!zstdwriter_write(zw, buf, size, err))
	    return error("zstdwriter_write", err), 1;
    }

    if (!zstdwriter_close(zw, err))
	return error("zstdwriter_close", err), 1;

    if (ferror(stdin))
	return fprintf(stderr, PROG ": stdin error\n"), 1;

    return 0;
}

// ex:set ts=8 sts=4 sw=4 noet:
