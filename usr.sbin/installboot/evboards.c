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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

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
