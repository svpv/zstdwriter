// Copyright (c) 2017 Alexey Tourbin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

// Writes exactly size bytes.
static bool xwrite(int fd, const void *buf, size_t size)
{
    assert(size);
    bool zero = false;
    do {
	ssize_t ret = write(fd, buf, size);
	if (ret < 0) {
	    if (errno == EINTR)
		continue;
	    return false;
	}
	if (ret == 0) {
	    if (zero) {
		// write(2) keeps returning zero
		errno = EAGAIN;
		return false;
	    }
	    zero = true;
	    continue;
	}
	zero = false;
	assert(ret <= size);
	buf = (char *) buf + ret;
	size -= ret;
    } while (size);

    return true;
}

#include <stdio.h> // sys_errlist

// A thread-safe strerror(3) replacement.
static const char *xstrerror(int errnum)
{
    // Some of the great minds say that sys_errlist is deprecated.
    // Well, at least it's thread-safe, and it does not deadlock.
    if (errnum > 0 && errnum < sys_nerr)
	return sys_errlist[errnum];
    return "Unknown error";
}

// Helpers to fill err[2] arg.
#define ERRNO(func) err[0] = func, err[1] = xstrerror(errno)
#define ERRZSTD(func, ret) err[0] = func, err[1] = ZSTD_getErrorName(ret)
#define ERRSTR(str) err[0] = __func__, err[1] = str

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include "zstdwriter.h"

struct zstdwriter {
    int fd;
    bool error;
    bool writeContentSize;
    bool writeChecksum;
    unsigned contentSize;
    off_t pos0;
    ZSTD_CStream *zcs;
    size_t zbufSize;
    unsigned char zbuf[];
};

struct zstdwriter *zstdwriter_fdopen(int fd, int compressionLevel, const char *err[2])
{
    ZSTD_compressionParameters cParams = ZSTD_getCParams(compressionLevel, 0, 0);
    return zstdwriter_fdopen_ex(fd, cParams, false, true, err);
}

struct zstdwriter *zstdwriter_fdopen_ex(int fd, ZSTD_compressionParameters cParams,
	bool writeContentSize, bool writeChecksum, const char *err[2])
{
    ZSTD_CStream *zcs = ZSTD_createCStream();
    if (!zcs)
	return ERRSTR("ZSTD_createCStream failed"), NULL;

    ZSTD_frameParameters fParams = { .checksumFlag = writeChecksum };
    ZSTD_parameters params = { cParams, fParams };
    size_t zret = ZSTD_initCStream_advanced(zcs, NULL, 0, params, 0);
    if (ZSTD_isError(zret))
	return ERRZSTD("ZSTD_initCStream_advanced", zret),
	       ZSTD_freeCStream(zcs), NULL;

    size_t zbufSize = ZSTD_CStreamOutSize();
    struct zstdwriter *zw = malloc(sizeof *zw + zbufSize);
    if (!zw)
	return ERRNO("malloc"),
	       ZSTD_freeCStream(zcs), NULL;

    memset(zw, 0, sizeof *zw);
    zw->fd = fd;
    zw->zcs = zcs;
    zw->zbufSize = zbufSize;

    if (writeContentSize) {
	zw->writeContentSize = true;
	zw->writeChecksum = writeChecksum;
	zw->pos0 = lseek(fd, 0, SEEK_CUR);
	if (zw->pos0 == -1)
	    return ERRNO("lseek"),
		   ZSTD_freeCStream(zcs), free(zw), NULL;
	// Content size is not yet known.
	if (!xwrite(zw->fd, "0123", 4))
	    return ERRNO("write"),
		   ZSTD_freeCStream(zcs), free(zw), NULL;
    }

    return zw;
}

bool zstdwriter_write(struct zstdwriter *zw, const void *buf, size_t size, const char *err[2])
{
    if (zw->error)
	return ERRSTR("previous write failed"), false;

    zw->contentSize += size;
    if (zw->writeContentSize && zw->contentSize < size)
	return ERRSTR("content size overflow"), false;

    ZSTD_inBuffer in = { buf, size, 0 };
    while (in.pos < in.size) {
	ZSTD_outBuffer out = { zw->zbuf, zw->zbufSize, 0 };
	size_t zret = ZSTD_compressStream(zw->zcs, &out, &in);
	if (ZSTD_isError(zret))
	    return ERRZSTD("ZSTD_compressStream", zret),
		   zw->error = true, false;
	if (out.pos && !xwrite(zw->fd, zw->zbuf, out.pos))
	    return ERRNO("write"),
		   zw->error = true, false;
    }
    assert(in.pos == size);
    return true;
}

static void justClose(struct zstdwriter *zw)
{
    close(zw->fd);
    ZSTD_freeCStream(zw->zcs);
    free(zw);
}

#include <endian.h>

bool zstdwriter_close(struct zstdwriter *zw, const char *err[2])
{
    if (zw->error)
	return ERRSTR("previous write failed"),
	       justClose(zw), false;

    while (1) {
	ZSTD_outBuffer out = { zw->zbuf, zw->zbufSize, 0 };
	size_t zret = ZSTD_endStream(zw->zcs, &out);
	if (ZSTD_isError(zret))
	    return ERRZSTD("ZSTD_endStream", zret),
		   justClose(zw), false;
	if (out.pos && !xwrite(zw->fd, zw->zbuf, out.pos))
	    return ERRNO("write"),
		   justClose(zw), false;
	if (zret == 0)
	    break;
    }

    if (!zw->writeContentSize)
	return justClose(zw), true;

    off_t pos = lseek(zw->fd, zw->pos0, SEEK_SET);
    if (pos == -1)
	return ERRNO("lseek"),
	       justClose(zw), false;
    assert(pos == zw->pos0);

    char frameHeader[9];
    unsigned magic = htole32(0xFD2FB528);
    unsigned contentSize = htole32(zw->contentSize);
    memcpy(frameHeader, &magic, 4);
    memcpy(frameHeader + 5, &contentSize, 4);
    frameHeader[4] = 0x80 | (zw->writeChecksum << 2);

    if (!xwrite(zw->fd, frameHeader, sizeof frameHeader))
	return ERRNO("write"),
	       justClose(zw), false;

    return justClose(zw), true;
}

// ex:set ts=8 sts=4 sw=4 noet:
