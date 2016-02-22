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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <netinet/in.h>

#include "./cap_fsemu.h"

/*
 * This file is intended to be compiled into the main program,
 * so that it will use these symbols instead of the ones in libc.
 *
 */
#ifdef DISABLED_FOR_NOW
int __attribute__ ((visibility ("default")))
socket(int domain, int type, int protocol)
{
	return rfs_socket(domain, type, protocol);
}

int __attribute__ ((visibility ("default")))
bind(int s, const struct sockaddr *addr, socklen_t addrlen)
{
	return rfs_bind(s, addr, addrlen);
}
#endif

int __attribute__ ((visibility ("default")))
chdir(const char *path)
{
	int dirfd = cap_fsemu_dir_lookup(path);
	if (dirfd < 0) abort();
	return cap_fsemu_chdir(path);
}

int __attribute__ ((visibility ("default")))
stat(const char *path, struct stat *sb)
{
	return cap_fsemu_stat(path, sb);
}

FILE * __attribute__ ((visibility ("default")))
fopen(const char * restrict path, const char * restrict mode)
{
	return cap_fsemu_fopen(path, mode);
}

int __attribute__ ((visibility ("default")))
scandir(const char *dirname, struct dirent ***namelist,
    int (*select)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **))
{
	/* TODO: implement this */
	return 0;
}

/*
 * XXX -- begin filesystem emulation code. This should be broken out into a
 * separate library, and not distributed along with the stubs.
 */

static int fopen_flags(const char *flags);
static int _cap_fsemu_dir_lookup_by_path(const char *path);
static int _cap_fsemu_path_lookup(const char *path, int mode);
static void _cap_die(const char *message, const char *path);

struct fsemu_dentry {
	char *path; /* absolute path */
	int mode; /* flags passed to open() */
	int fd; /* descriptor */
	LIST_ENTRY(fsemu_dentry) le;
};
LIST_HEAD(, fsemu_dentry) cfe_namespace;
char cfe_wd_path[MAXPATHLEN + 1]; /* current working directory */

int cap_fsemu_init()
{
	LIST_INIT(&cfe_namespace);
	memset(&cfe_wd_path, 0, sizeof(cfe_wd_path));
	(void) getcwd((char *)&cfe_wd_path, sizeof(cfe_wd_path));
	return 0;
}

/* "Mount" a directory from the global namespace into the emulated
 * per-process namespace. Directories can be mounted read-only (O_RDONLY)
 * or read-write (O_RDWR).
 */
int cap_fsemu_mount(const char *src, const char *dst, int mode)
{
	int fd = -1;
	struct fsemu_dentry *dentry = malloc(sizeof(*dentry));

	if (mode != O_RDONLY && mode != O_RDWR) {
		errno = EINVAL;
		goto err_out;
	}
	fd = open(src, O_DIRECTORY | mode);
	if (fd < 0)
		goto err_out;

	if (!dentry)
		goto err_out;
	dentry->path = strdup(dst);
	dentry->mode = mode;
	dentry->fd = fd;
	if (!dentry->path)
		goto err_out;

	/* TODO: check for overlap with existing entries, and reject(?) overlaps */
	LIST_INSERT_HEAD(&cfe_namespace, dentry, le);

	return 0;

err_out:
	if (dentry) free(dentry->path);
	free(dentry);
	if (fd >= 0) (void) close(fd);
	return -1;
}

int cap_fsemu_dir_lookup(const char *path)
{
	struct fsemu_dentry *dentry = NULL;

	LIST_FOREACH(dentry, &cfe_namespace, le) {
		if (strcmp(dentry->path, path) == 0)
			return dentry->fd;
	}
	return -1;
}

int cap_fsemu_chdir(const char *path)
{
	//TODO: validation
	(void)strncpy((char *)&cfe_wd_path, path, sizeof(cfe_wd_path));
	return 0;
}

int cap_fsemu_stat(const char *path, struct stat *sb)
{
	int dirfd = _cap_fsemu_dir_lookup_by_path(path);
	if (dirfd < 0)
		_cap_die("directory lookup failed", path);
	int retval = fstatat(dirfd, path, sb, 0);
	//(void) close(fd);
	return retval;
}

FILE *
cap_fsemu_fopen(const char * restrict path, const char * restrict mode)
{
	int fd = _cap_fsemu_path_lookup(path, fopen_flags(mode));
	if (fd < 0)
		return NULL;

	return fdopen(fd, mode);
}

/* Given flags in the fopen(3) style, convert them to open(2) style */
static int
fopen_flags(const char *flags)
{
	// FIXME: this does not cover all of the possible flags
	if (flags[0] == 'r') {
		return O_RDONLY;
	} else if (flags[0] == 'w') {
		return O_WRONLY;
	} else if (flags[0] == 'a') {
		return (O_WRONLY | O_APPEND);
	}
	_cap_die("Bad flags argument", flags);
	return -1;
}

/* Given a full path, e.g. /etc/passwd, return a file descriptor
 * to the parent directory of the file.
 */
static int
_cap_fsemu_dir_lookup_by_path(const char *path)
{
	int dirfd;

	/* FIXME: dirname() is not threadsafe */
	char *dirn = dirname(path);
	//puts(dirn); abort();
	if (dirn[0] == '.' && dirn[1] == '\0') {
		dirfd = cap_fsemu_dir_lookup(cfe_wd_path);
	} else {
		dirfd = cap_fsemu_dir_lookup(dirn);
	}
	if (dirfd < 0)
		_cap_die("directory not found", path);
	return dirfd;
}

static int
_cap_fsemu_path_lookup(const char *path, int mode)
{
	//char path[MAXPATHLEN + 1];
	int dirfd;
	int fd = -1;

	if (path[0] == '/') {
		/* Handle absolute paths */
		_cap_die("Absolute paths not implemented yet", path);
	} else if (path[0] == '.' && path[1] == '.') {
		_cap_die("TODO; relative paths above CWD", path);
	} else {
		/* Handle filenames in the current working directory */
		dirfd = cap_fsemu_dir_lookup(cfe_wd_path);
		if (dirfd < 0)
			_cap_die("directory not found", path);

		fd = openat(dirfd, path, mode);
	}

	return fd;
}

static void
_cap_die(const char *message, const char *path)
{
	fprintf(stderr, "*** Filesystem access violation *** %s: %s", path, message);
	abort();
}
