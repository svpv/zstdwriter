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

#pragma once

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

struct zstdwriter *zstdwriter_fdopen(int fd, int compressionLevel,
				     const char *err[2]) __attribute__((nonnull));

#ifdef ZSTD_H_ZSTD_STATIC_LINKING_ONLY
struct zstdwriter *zstdwriter_fdopen_ex(int fd, ZSTD_compressionParameters cParams,
					bool writeContentSize, bool writeChecksum,
					const char *err[2]) __attribute__((nonnull));
#endif

bool zstdwriter_write(struct zstdwriter *zw, const void *buf, size_t size, const char *err[2]) __attribute__((nonnull));
bool zstdwriter_close(struct zstdwriter *zw, const char *err[2]) __attribute__((nonnull));

#ifdef __cplusplus
}
#endif
