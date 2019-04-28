/*	$NetBSD$	*/

/*-
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(__lint)
__RCSID("$NetBSD$");
#endif  /* !__lint */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "installboot.h"
#include "evboards.h"

#ifndef EVBOARDS_PLIST_BASE
#define	EVBOARDS_PLIST_BASE	"/usr"
#endif

static const char evb_plist_default_location[] =
    EVBOARDS_PLIST_BASE "/share/installboot";

static const char *
evb_plist_default_path(ib_params *params, char *buf, size_t bufsize)
{
	int ret = snprintf(buf, bufsize, "%s/%s_boards.plist",
			   evb_plist_default_location, params->machine->name);
	if (ret < 0 || (size_t)ret >= bufsize)
		return NULL;

	return buf;
}

prop_dictionary_t
evb_plist_load(ib_params *params, const char *path)
{
	char default_path[PATH_MAX+1];

	if (path == NULL) {
		if ((path = evb_plist_default_path(params, default_path,
					sizeof(default_path))) == NULL) {
			return NULL;
		}
	}

	return prop_dictionary_internalize_from_file(path);
}

/*
 * Top-level dictionary helpers.
 */
const char *
evb_plist_soc_name(prop_dictionary_keysym_t key)
{
	if (prop_object_type(key) != PROP_TYPE_DICT_KEYSYM)
		return NULL;
	
	return prop_dictionary_keysym_cstring_nocopy(key);
}

prop_array_t
evb_plist_soc_boards(prop_dictionary_t plist, const char *socname)
{
	if (prop_object_type(plist) != PROP_TYPE_DICTIONARY)
		return NULL;

	prop_array_t ret = prop_dictionary_get(plist, socname);
	if (ret == NULL || prop_object_type(ret) != PROP_TYPE_ARRAY)
		return NULL;
	
	return ret;
}

static const char *
evb_plist_soc_for_board(ib_params *params)
{
	const char *socname;

	assert(params->flags & IB_BOARD);

	if (params->mach_data == NULL) {
		warnx("Unable to map board '%s' to an SoC.", params->board);
		return NULL;
	}

	prop_dictionary_t board = evb_plist_lookup_board(params->mach_data,
							 params->board,
							 &socname);
	if (board == NULL) {
		warnx("Unknown board '%s'.", params->board);
		return NULL;
	}

	assert(socname != NULL);
	return socname;
}

static const char *
evb_plist_soc_from_params(ib_params *params)
{
	const char *socname = NULL;

	if (params->flags & IB_SOC) {
		socname = params->soc;
	} else if (params->flags & IB_BOARD) {
		socname = evb_plist_soc_for_board(params);
	} else {
		warnx("Must specify board or soc.");
	}

	return socname;
}

/*
 * Board object helpers.
 */
static const char evb_plist_board_name_key[] = "board-name";
static const char evb_plist_board_description_key[] = "description";

const char *
evb_plist_board_name(prop_dictionary_t board)
{
	if (prop_object_type(board) != PROP_TYPE_DICTIONARY)
		return NULL;
	
	prop_string_t str = prop_dictionary_get(board,
	    evb_plist_board_name_key);
	if (str == NULL || prop_object_type(str) != PROP_TYPE_STRING)
		return NULL;

	return prop_string_cstring_nocopy(str);
}

const char *
evb_plist_board_description(prop_dictionary_t board)
{
	if (prop_object_type(board) != PROP_TYPE_DICTIONARY)
		return NULL;
	
	prop_string_t str = prop_dictionary_get(board,
	    evb_plist_board_description_key);
	if (str == NULL || prop_object_type(str) != PROP_TYPE_STRING)
		return NULL;
	
	return prop_string_cstring_nocopy(str);
}

/*
 * High-level helpers.
 */
prop_dictionary_t
evb_plist_lookup_board(prop_dictionary_t plist, const char * const boardname,
    const char **socnamep)
{
	prop_dictionary_t ret = NULL;

	prop_object_iterator_t top_iter = prop_dictionary_iterator(plist);
	if (top_iter == NULL)
		return NULL;

	prop_dictionary_keysym_t soc_key;
	while ((soc_key = prop_object_iterator_next(top_iter)) != NULL) {
		const char *cursoc = evb_plist_soc_name(soc_key);
		if (cursoc == NULL)
			continue;

		prop_array_t boards = evb_plist_soc_boards(plist, cursoc);
		prop_object_iterator_t soc_iter = prop_array_iterator(boards);
		prop_dictionary_t board;
		while ((board = prop_object_iterator_next(soc_iter)) != NULL) {
			const char *curboard = evb_plist_board_name(board);
			if (curboard != NULL &&
			    strcmp(boardname, curboard) == 0) {
				if (socnamep != NULL)
					*socnamep = cursoc;
				ret = board;
				break;
			}
		}
		prop_object_iterator_release(soc_iter);
		if (board != NULL)
			break;
	}

 out:
	prop_object_iterator_release(top_iter);
	return ret;
}

void
evb_plist_list_boards(prop_dictionary_t plist, FILE *out)
{
	prop_object_iterator_t top_iter = prop_dictionary_iterator(plist);
	if (top_iter == NULL) {
		fprintf(stderr, "Unable to read property list.\n");
		return;
	}

	prop_dictionary_keysym_t soc_key;
	while ((soc_key = prop_object_iterator_next(top_iter)) != NULL) {
		const char *cursoc = evb_plist_soc_name(soc_key);
		if (cursoc == NULL)
			continue;

		fprintf(out, "SoC \"%s\":\n", cursoc);

		prop_array_t boards = evb_plist_soc_boards(plist, cursoc);
		prop_object_iterator_t soc_iter = prop_array_iterator(boards);
		prop_dictionary_t board;
		while ((board = prop_object_iterator_next(soc_iter)) != NULL) {
			const char *curboard = evb_plist_board_name(board);
			const char *curdesc =
			    evb_plist_board_description(board);

			if (curboard == NULL || curdesc == NULL)
				continue;

			fprintf(out, "     %-25s %s\n", curboard, curdesc);
		}
		prop_object_iterator_release(soc_iter);
	}

	prop_object_iterator_release(top_iter);
}

/*
 * Board method helpers.
 */
static const struct evboard_methods *
evb_methods_lookup_byname(const char *name,
    const struct evboard_methods * const * tab)
{
	const struct evboard_methods *m;
	int i;

	for (i = 0; tab[i] != NULL; i++) {
		m = tab[i];
		if (strcmp(name, m->name) == 0)
			return m;
	}
	return NULL;
}

const struct evboard_methods *
evb_methods_lookup(ib_params *params,
    const struct evboard_methods * const * tab)
{
	char compound_name[128];
	const char *socname = NULL;
	const struct evboard_methods *m = NULL;
	int ret;

	socname = evb_plist_soc_from_params(params);
	if (socname == NULL)
		return NULL;

	if (params->flags & IB_SOC) {
		if ((m = evb_methods_lookup_byname(socname, tab)) == NULL)
			warnx("No methods for SoC '%s'.", params->soc);
		return m;
	}

	assert(params->flags & IB_BOARD);

	/* First -- try <soc>-<board>. */
	ret = snprintf(compound_name, sizeof(compound_name), "%s-%s",
	    socname, params->board);
	if (ret < 0 || (size_t)ret >= sizeof(compound_name))
		goto out;
	if ((m = evb_methods_lookup_byname(compound_name, tab)) != NULL)
		goto out;

	/* And now just <soc>. */
	if ((m = evb_methods_lookup_byname(socname, tab)) == NULL)
		warnx("No methods for '%s' or '%s'.", compound_name, socname);

 out:
	return m;
}

/*
 * U-boot helpers.
 */
static const char evb_uboot_default_location[] = "/usr/pkg/share";

const char *
evb_uboot_base(ib_params *params, char *buf, size_t bufsize)
{
	struct stat sb;
	int ret;

	/*
	 * If the user specified a stage1, and it's a directory,
	 * then that's where the u-boot binaries are.
	 */
	if (params->stage1 != NULL) {
		if (!S_ISDIR(params->s1stat.st_mode)) {
			warnx("%s: %s", params->stage1, strerror(ENOTDIR));
			return NULL;
		}
		ret = snprintf(buf, bufsize, "%s", params->stage1);
		if (ret < 0 || (size_t)ret > bufsize)
			return NULL;
		return buf;
	}

	/*
	 * User must specify a board type if the u-boot location was
	 * not specified.
	 */
	if (!(params->flags & IB_BOARD)) {
		errx(EXIT_FAILURE, "Must specify either the u-boot location "
		     "or the board type");
	}

	ret = snprintf(buf, bufsize, "%s/u-boot-%s",
	    evb_uboot_default_location, params->board);

	if (stat(buf, &sb) < 0) {
		if (errno == ENOENT) {
			warnx("Please install sysutils/u-boot-%s from pkgsrc.",
			    params->board);
		} else {
			warn("%s", buf);
		}
		return NULL;
	}
	if (!S_ISDIR(sb.st_mode)) {
		warnx("%s: %s", buf, strerror(ENOTDIR));
		return NULL;
	}

	return buf;
}

static const char *
evb_uboot_file_path(const char *uboot_base, const char *filename,
		    char *buf, size_t bufsize)
{
	int ret;

	ret = snprintf(buf, bufsize, "%s/%s", uboot_base, filename);
	if (ret < 0 || (size_t)ret >= bufsize)
		return NULL;

	return buf;
}

static int
evb_uboot_write_blob(ib_params *params, const char *uboot_file,
    const struct evboard_uboot_desc *desc)
{
	struct stat sb;
	int ifd = -1;
	char *blockbuf;
	int rv = 0;
	size_t thisblock;
	off_t curoffset;
	off_t remaining;

	blockbuf = malloc(params->sectorsize);
	if (blockbuf == NULL)
		goto out;

	ifd = open(uboot_file, O_RDONLY);
	if (ifd < 0) {
		warn("open '%s'", uboot_file);
		goto out;
	}
	if (fstat(ifd, &sb) < 0) {
		warn("fstat '%s'", uboot_file);
		goto out;
	}

	if (desc->file_size)
		remaining = desc->file_size;
	else
		remaining = sb.st_size - desc->file_offset;

	if (params->flags & IB_VERBOSE) {
		if (desc->file_offset) {
			printf("Writing '%s' -- %lld @ %lld ==> %lld\n",
			    desc->filename, (long long)remaining,
			    desc->file_offset, (long long)desc->image_offset);
		} else {
			printf("Writing '%s' -- %lld ==> %lld\n",
			    desc->filename, (long long)remaining,
			    (long long)desc->image_offset);
		}
	}

	if (lseek(ifd, desc->file_offset, SEEK_SET) < 0) {
		warn("lseek '%s' @ %lld", uboot_file,
		    (long long)desc->file_offset);
		goto out;
	}

	for (curoffset = desc->image_offset; remaining != 0;
	     remaining -= thisblock, curoffset += params->sectorsize) {
		thisblock = params->sectorsize;
		if (thisblock > remaining)
			thisblock = (size_t)remaining;
		if ((thisblock % params->sectorsize) != 0) {
			memset(blockbuf, 0, params->sectorsize);
			if (params->flags & UB_PRESERVE) {
				if (params->flags & IB_VERBOSE) {
					printf("(Reading '%s' -- %u @ %lld)\n",
					    params->filesystem,
					    params->sectorsize,
					    (long long)curoffset);
				}
				if (pread(params->fsfd, blockbuf,
					  params->sectorsize, curoffset) < 0) {
					warn("pread '%s'", params->filesystem);
					goto out;
				}
			}
		}
		if (read(ifd, blockbuf, thisblock) != (ssize_t)thisblock) {
			warn("read '%s'", uboot_file);
			goto out;
		}
		if (pwrite(params->fsfd, blockbuf, params->sectorsize,
			   curoffset) != (ssize_t)params->sectorsize) {
			warn("pwrite '%s'", params->filesystem);
			goto out;
		}
	}

	/* Success! */
	rv = 1;

 out:
	if (ifd != -1 && close(ifd) == -1)
		warn("close '%s'", uboot_file);
	if (blockbuf)
		free(blockbuf);
	return rv;
}

int
evb_uboot_setboot(ib_params *params, const struct evboard_uboot_desc *descs)
{
	char uboot_base_pathbuf[PATH_MAX+1];
	const char *uboot_base;
	char uboot_file_pathbuf[PATH_MAX+1];
	const char *uboot_file;
	struct stat sb;
	off_t max_offset = 0;
	int i;

	uboot_base = evb_uboot_base(params, uboot_base_pathbuf,
				    sizeof(uboot_base_pathbuf));
	if (uboot_base == NULL)
		return 0;

	/*
	 * First, make sure the files are all there.  While we're
	 * at it, calculate the largest byte offset that we will
	 * be writing.
	 */
	for (i = 0; descs[i].filename != NULL; i++) {
		uboot_file = evb_uboot_file_path(uboot_base,
		    descs[i].filename, uboot_file_pathbuf,
		    sizeof(uboot_file_pathbuf));
		if (uboot_file == NULL)
			return 0;
		if (stat(uboot_file, &sb) < 0) {
			warn("%s", uboot_file);
			return 0;
		}
		if (!S_ISREG(sb.st_mode)) {
			warnx("%s: %s", uboot_file, strerror(EFTYPE));
			return 0;
		}
		off_t this_max = (sb.st_size - descs[i].file_offset) +
		    descs[i].image_offset;
		if (max_offset < this_max)
			max_offset = this_max;
	}

	/*
	 * Ok, we've verified that all of the files are there, and now
	 * max_offset points to the first byte that's available for a
	 * partition containing a file system.
	 */

	/* XXX Check MBR table for overlapping partitions. */

	/*
	 * Now write each binary component to the appropriate location
	 * on disk.
	 */
	for (i = 0; descs[i].filename != NULL; i++) {
		uboot_file = evb_uboot_file_path(uboot_base,
		    descs[i].filename, uboot_file_pathbuf,
		    sizeof(uboot_file_pathbuf));
		if (uboot_file == NULL)
			return 0;
		if (!evb_uboot_write_blob(params, uboot_file, &descs[i]))
			return 0;
	}

	return 1;
}
