/*
 * Copyright (c) 2016 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef _CAP_FSEMU_H

struct stat;

int cap_fsemu_init(void);
int cap_fsemu_mount(const char *src, const char *dst, int mode);
int cap_fsemu_dir_lookup(const char *path);
int cap_fsemu_chdir(const char *path);
int cap_fsemu_stat(const char *path, struct stat *sb);
FILE * cap_fsemu_fopen(const char * restrict path, const char * restrict mode);

int
cap_fsemu_scandir(const char *dirname, struct dirent ***namelist,
    int (*select)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **));

#endif /* !_CAP_FSEMU_H */
