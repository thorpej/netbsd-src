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

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "installboot.h"
#include "evboards.h"

/*
 * The board database is implemented as a property list.  The base
 * system provides a set of known boards, keyed by their "compatible"
 * device tree property.
 *
 * The database provided by the base system is meant to help guide
 * the user as to which u-boot package needs to be installed on the
 * system in order to write the boot loader to the boot media.  The
 * base board plist is specific to the $MACHINE (e.g. "evbarm"), and
 * is installed along with the build tools, e.g.:
 *
 * (native location)
 *	/usr/sbin/installboot
 *	/usr/share/installboot/evbarm/boards.plist
 *	/usr/share/installboot/evbmips/boards.plist
 *
 * (example cross host tool location)
 *	/usr/local/xnbsd/bin/nbinstallboot
 *	/usr/local/xnbsd/share/installboot/evbarm/boards.plist
 *	/usr/local/xnbsd/share/installboot/evbmips/boards.plist
 *
 * The schema of the base board plist is as follows:
 *
 * <plist>
 * <dict>
 *	<!--
 *	  -- Key: string matching a "compatible" DT property.
 *	  -- Value: dictionary representing a board object.
 *	  -- (required)
 *	  -->
 *	<key>example,example-board</key>
 *	<dict>
 *		<!--
 *		  -- Key: "description".
 *		  -- Value: string containing the board description.
 *		  -- (required)
 *		  -->
 *		<key>description</key>
 *		<string>Example Co. Example Board</string>
 *
 *		<!--
 *		  -- Key: "u-boot-pkg".
 *		  -- Value: string representing the board-specific
 *		  --        portion of the u-boot package name.
 *		  --        In this example, the package's full name
 *		  --        is "u-boot-exampleboard".  This is used
 *		  --        to recommend to the user which u-boot
 *		  --        package to install.  If not present, then
 *		  --        no package recommendation will be made.
 *		  -- (optional)
 *		  -->
 *		<key>u-boot-pkg</key>
 *		<string>exampleboard</string>
 *	</dict>
 * </dict>
 * </plist>
 *
 * Individual u-boot packages install their own overlay property list
 * files that installboot(8) then scans for.  These overlay files are
 * named "installboot.plist", and are installed alongside the u-boot
 * binaries by the individual u-boot packages, for example:
 *
 *	/usr/pkg/share/u-boot/exampleboard/installboot.plist
 *	/usr/pkg/share/u-boot/exampleboard/u-boot-with-spl.bin
 *
 * installboot(8) scans a set of directories looking for "installboot.plist"
 * overlay files one directory deep.  For example:
 *
 *	/usr/pkg/share/u-boot/
 *				exampleboard/installboot.plist
 *				superarmdeluxe/installboot.plist
 *				dummy/
 *
 * In this example, "/usr/pkg/share/u-boot" is scanned, it would identify
 * "exampleboard" and "superarmdeluxe" as directories containing overlays
 * and load them.
 *
 * The default path scanned for u-boot packages is:
 *
 *	/usr/pkg/share/u-boot
 *
 * This can be overridden with the INSTALLBOOT_UBOOT_PATHS environment
 * variable, which contains a colon-sparated list of directories, e.g.:
 *
 *	/usr/pkg/share/u-boot:/home/jmcneill/hackityhack/u-boot
 *
 * The scan only consults the top-level children of the specified directory.
 *
 * Each overlay includes complete board objects that entirely replace
 * the system-provided board objects in memory.  Some of the keys in
 * overlay board objects are computed at run-time and should not appear
 * in the plists loaded from the file system.
 *
 * The schema of the overlay board plists are as follows:
 *
 * <plist>
 * <dict>
 *	<!--
 *	  -- Key: string matching a "compatible" DT property.
 *	  -- Value: dictionary representing a board object.
 *	  -- (required)
 *	  -->
 *	<key>example,example-board</key>
 *	<dict>
 *		<!--
 *		  -- Key: "description".
 *		  -- Value: string containing the board description.
 *		  -- (required)
 *		  -->
 *		<key>description</key>
 *		<string>Example Co. Example Board</string>
 *
 *		<!--
 *		  -- Key: "u-boot-install".
 *		  --      (and variants; see discussion below)
 *		  --      "u-boot-install-emmc", etc.).
 *		  -- Value: Array of u-boot installation step objects,
 *		  --        as described below.
 *		  -- (required)
 *		  --
 *		  -- At least one of these objects is required.  If the
 *		  -- board uses a single set of steps for all boot media
 *		  -- types, then it should provide just "u-boot-install".
 *		  -- Otherwise, it whould provide one or more objects
 *		  -- with names reflecting the media type, e.g.:
 *		  --
 *		  --	"u-boot-install-sd"	(for SD cards)
 *		  --	"u-boot-install-emmc"	(for eMMC modules)
 *		  --	"u-boot-install-usb"	(for USB block storage)
 *		  --
 *		  -- These installation steps will be selectable using
 *		  -- the "media=..." option to installboot(8).
 *		  -->
 *		<key>u-boot-install</key>
 *		<array>
 *			<!-- see installation object discussion below. -->
 *		</array>
 *
 *		<!--
 *		  -- Key: "runtime-u-boot-path"
 *		  -- Value: A string representing the path to the u-boot
 *		  --        binary files needed to install the boot loader.
 *		  --        This value is computed at run-time and is the
 *		  --        same directory in which the instalboot.plist
 *		  --        file for that u-boot package is located.
 *		  --        This key/value pair should never be included
 *		  --        in an installboot.plist file, and any value
 *		  --        appearing in installboot.plist will be ignored.
 *		  -- (computed at run-time)
 *		  -->
 *		<key>runtime-u-boot-path</key>
 *		<string>/usr/pkg/share/u-boot/exampleboard</string>
 *	</dict>
 * </dict>
 * </plist>
 *
 * The installation objects provide a description of the steps needed
 * to install u-boot on the boot media.  Each installation object it
 * itself an array of step object.
 *
 * A basic installation object has a single step that instructs
 * installboot(8) to write a file to a specific offset onto the
 * boot media.
 *
 *	<key>u-boot-install</key>
 *	<!-- installation object -->
 *	<array>
 *		<!-- step object -->
 *		<dict>
 *			<!--
 *			  -- Key: "file-name".
 *			  -- Value: a string naming the file to be
 *			  --        written to the media.
 *			  -- (required)
 *			  -->
 *			<key>file-name</key>
 *			<string>u-boot-with-spl.bin</string>
 *
 *			<!--
 *			  -- Key: "image-offset".
 *			  -- Value: an integer specifying the offset
 *			  --        into the output image or device
 *			  --        where to write the file.  Defaults
 *			  --        to 0 if not specified.
 *			  -- (optional)
 *			  -->
 *			<key>image-offset</key>
 *			<integer>8192</integer>
 *		</dict>
 *	</array>
 *
 * Some installations require multiple steps with special handling.
 *
 *	<key>u-boot-install</key>
 *	<array>
 *		<--
 *		 -- Step 1: Write the initial portion of the boot
 *		 -- loader onto the media.  The loader has a "hole"
 *		 -- to leave room for the MBR partition table.  Take
 *		 -- care not to scribble over the table.
 *		 -->
 *		<dict>
 *			<key>file-name</key>
 *			<string>u-boot-img.bin</string>
 *
 *			<!--
 *			  -- Key: "file-size".
 *			  -- Value: an integer specifying the amount of
 *			  --        data from the file to be written to the
 *			  --        output.  Defaults to "to end of file" if
 *			  --        not specified.
 *			  -- (optional)
 *			  -->
 *			<!-- Stop short of the MBR partition table. -->
 *			<key>file-size</key>
 *			<integer>442</integer>
 *
 *			<!--
 *			  -- Key: "preserve".
 *			  -- Value: a boolean indicating that any partial
 *			  --        output block should preserve any pre-
 *			  --        existing contents of that block for
 *			  --        the portion of the of the block not
 *			  --        overwritten by the input file.
 *			  --        (read-modify-write)
 *			  -- (optional)
 *			  -->
 *			<!-- Preserve the MBR partition table. -->
 *			<key>preserve</key>
 *			<true/>
 *		</dict>
 *		<--
 *		 -- Step 2: Write the rest of the loader after the
 *		 -- MBR partition table.
 *		 -->
 *		<dict>
 *			<key>file-name</key>
 *			<string>u-boot-img.bin</string>
 *
 *			<!--
 *			  -- Key: "file-offset".
 *			  -- Value: an integer specifying the offset into
 *			  --        the input file from where to start
 *			  --        copying to the output.
 *			  -- (optional)
 *			  -->
 *			<key>file-offset</key>
 *			<integer>512</integer>
 *
 *			<!-- ...just after the MBR partition talble. -->
 *			<key>image-offset</key>
 *			<integer>512</integer>
 *		</dict>
 *	</array>
 */

/*
 * make_path --
 *	Build a path into the given buffer with the specified
 *	format.  Returns NULL if the path won't fit.
 */
const char *
make_path(char *buf, size_t bufsize, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsnprintf(buf, bufsize, fmt, ap);
	va_end(ap);

	if (ret < 0 || (size_t)ret >= bufsize)
		return NULL;

	return buf;
}

#ifndef EVBOARDS_PLIST_BASE
#define	EVBOARDS_PLIST_BASE	"/usr"
#endif

static const char evb_db_base_location[] =
    EVBOARDS_PLIST_BASE "/share/installboot";

#ifndef DEFAULT_UBOOT_PKG_PATH
#define	DEFAULT_UBOOT_PKG_PATH	"/usr/pkg/share/u-boot"
#endif

#ifndef UBOOT_PATHS_ENV_VAR
#define	UBOOT_PATHS_ENV_VAR	"INSTALLBOOT_UBOOT_PATHS"
#endif

static const char evb_uboot_pkg_path[] = DEFAULT_UBOOT_PKG_PATH;

/*
 * evb_db_base_path --
 *	Returns the path to the base board db file.
 */
static const char *
evb_db_base_path(ib_params *params, char *buf, size_t bufsize)
{

	return make_path(buf, bufsize, "%s/%s/boards.plist",
	    evb_db_base_location, params->machine->name);
}

/*
 * evb_uboot_pkg_paths --
 *	Returns an array of u-boot package paths to scan for
 *	installboot.plist files.
 *
 *	Number of array elements, not including the NULL terminator,
 *	is returned in *countp.
 *
 *	The working buffer is returned in *bufp so that the caller
 *	can free it.
 */
char **
evb_uboot_pkg_paths(ib_params *params, int *countp, void **bufp)
{
	char **ret_array = NULL;
	char *buf = NULL;
	const char *pathspec;
	int i, count = 0;
	char *cp;

	pathspec = getenv(UBOOT_PATHS_ENV_VAR);
	if (pathspec == NULL)
		pathspec = evb_uboot_pkg_path;

	if (strlen(pathspec) == 0)
		goto out;

	/* Count the path elements. */
	for (cp = __UNCONST(pathspec); cp != NULL;
	     cp = strchr(cp, ':'), count++);

	buf = malloc((sizeof(char *) * (count + 1)) +
		     strlen(pathspec) + 1);
	if (buf == NULL)
		goto out;

	ret_array = (char **)buf;
	cp = buf + (sizeof(char *) * (count + 1));
	strcpy(cp, pathspec); /* this is a safe strcpy(); don't replace it. */
	for (i = 0;;) {
		ret_array[i++] = cp;
		cp = strchr(cp, ':');
		if (cp == NULL)
			break;
		*cp = '\0';
	}
	assert(i == count);
	ret_array[i] = NULL;

 out:
	if (ret_array == NULL) {
		if (buf != NULL)
			free(buf);
	} else {
		if (countp != NULL)
			*countp = count;
		if (bufp != NULL)
			*bufp = buf;
	}
	return ret_array;
}

static const char step_file_name_key[] = "file-name";
static const char step_file_offset_key[] = "file-offset";
static const char step_file_size_key[] = "file-size";
static const char step_image_offset_key[] = "image-offset";
static const char step_preserve_key[] = "preserve";

static bool
validate_ubstep_object(evb_ubstep obj)
{
	/*
	 * evb_ubstep is a dictionary with the following keys:
	 *
	 *	"file-name"	(string) (required)
	 *	"file-offset"	(number) (optional)
	 *	"file-size"	(number) (optional)
	 *	"image-offset"	(number) (optional)
	 *	"preserve"	(bool)	 (optional)
	 */
	if (prop_object_type(obj) != PROP_TYPE_DICTIONARY)
		return false;

	prop_object_t v;

	v = prop_dictionary_get(obj, step_file_name_key);
	if (v == NULL ||
	    prop_object_type(v) != PROP_TYPE_STRING)
	    	return false;

	v = prop_dictionary_get(obj, step_file_offset_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_NUMBER)
	    	return false;

	v = prop_dictionary_get(obj, step_file_size_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_NUMBER)
	    	return false;

	v = prop_dictionary_get(obj, step_image_offset_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_NUMBER)
	    	return false;

	v = prop_dictionary_get(obj, step_preserve_key);
	if (v != NULL &&
	    prop_object_type(v) != PROP_TYPE_BOOL)
	    	return false;

	return true;
}

static bool
validate_ubinstall_object(evb_ubinstall obj)
{
	/*
	 * evb_ubinstall is an array with one or more evb_ubstep
	 * objects.
	 *
	 * (evb_ubsteps is just a convenience type for iterating
	 * over the steps.)
	 */
	if (prop_object_type(obj) != PROP_TYPE_ARRAY)
		return false;
	if (prop_array_count(obj) < 1)
		return false;

	prop_object_t v;
	prop_object_iterator_t iter = prop_array_iterator(obj);

	while ((v = prop_object_iterator_next(iter)) != NULL) {
		if (!validate_ubstep_object(v))
			break;
	}

	prop_object_iterator_release(iter);
	return v == NULL;
}

static const char board_description_key[] = "description";
static const char board_u_boot_pkg_key[] = "u-boot-pkg";
static const char board_u_boot_path_key[] = "runtime-u-boot-path";
static const char board_u_boot_install_key[] = "u-boot-install";

static bool
validate_board_object(evb_board obj, bool is_overlay)
{
	/*
	 * evb_board is a dictionary with the following keys:
	 *
	 *	"description"		(string) (required)
	 *	"u-boot-pkg"		(string) (optional, base only)
	 *	"runtime-u-boot-path"	(string) (required, overlay only)
	 *
	 * With special consideration for these keys:
	 *
	 * Either this key and no other "u-boot-install*" keys:
	 *	"u-boot-install"	(string) (required, overlay only)
	 *
	 * Or one or more keys of the following pattern:
	 *	"u-boot-install-*"	(string) (required, overlay only)
	 */
	bool has_default_install = false;
	bool has_media_install = false;

	if (prop_object_type(obj) != PROP_TYPE_DICTIONARY)
		return false;

	prop_object_t v;

	v = prop_dictionary_get(obj, board_description_key);
	if (v == NULL ||
	    prop_object_type(v) != PROP_TYPE_STRING)
	    	return false;

	v = prop_dictionary_get(obj, board_u_boot_pkg_key);
	if (v != NULL &&
	    (is_overlay || prop_object_type(v) != PROP_TYPE_STRING))
	    	return false;

	/*
	 * "runtime-u-boot-path" is added to an overlay after we've
	 * validated the board object, so simply make sure it's not
	 * present.
	 */
	v = prop_dictionary_get(obj, board_u_boot_path_key);
	if (v != NULL)
		return false;

	prop_object_iterator_t iter = prop_dictionary_iterator(obj);
	prop_dictionary_keysym_t key;
	while ((key = prop_object_iterator_next(iter)) != NULL) {
		const char *cp = prop_dictionary_keysym_cstring_nocopy(key);
		if (strcmp(cp, board_u_boot_install_key) == 0) {
			has_default_install = true;
		} else if (strncmp(cp, board_u_boot_install_key,
				   sizeof(board_u_boot_install_key) - 1) == 0 &&
			   cp[sizeof(board_u_boot_install_key) - 1] == '-') {
			has_media_install = true;
		} else {
			continue;
		}
		v = prop_dictionary_get_keysym(obj, key);
		assert(v != NULL);
		if (!is_overlay || !validate_ubinstall_object(v))
			break;
	}
	prop_object_iterator_release(iter);
	if (key != NULL)
		return false;

	/*
	 * Overlays must have only a default install key OR one or more
	 * media install keys.
	 */
	if (is_overlay)
		return has_default_install ^ has_media_install;

	/*
	 * Base board objects must have neither.
	 */
	return (has_default_install | has_media_install) == false;
}

/*
 * evb_db_load_overlay --
 *	Load boards from an overlay file into the db.
 */
static void
evb_db_load_overlay(ib_params *params, const char *path,
    const char *runtime_uboot_path)
{
	prop_dictionary_t overlay;
	struct stat sb;

	if (params->flags & IB_VERBOSE)
		printf("Loading '%s'.\n", path);

	if (stat(path, &sb) < 0) {
		warn("'%s'", path);
		return;
	} else {
		overlay = prop_dictionary_internalize_from_file(path);
		if (overlay == NULL) {
			warnx("unable to parse overlay '%s'", path);
			return;
		}
	}

	/*
	 * Validate all of the board objects and add them to the board
	 * db, replacing any existing entries as we go.
	 */
	prop_object_iterator_t iter = prop_dictionary_iterator(overlay);
	prop_dictionary_keysym_t key;
	prop_dictionary_t board;
	while ((key = prop_object_iterator_next(iter)) != NULL) {
		board = prop_dictionary_get_keysym(overlay, key);
		assert(board != NULL);
		if (!validate_board_object(board, false)) {
			warnx("invalid board object in '%s': '%s'", path,
			    prop_dictionary_keysym_cstring_nocopy(key));
			continue;
		}

		/* Add "runtime-u-boot-path". */
		prop_string_t string =
		    prop_string_create_cstring(runtime_uboot_path);
		assert(string != NULL);
		prop_dictionary_set(overlay, board_u_boot_path_key, string);
		prop_object_release(string);

		/* Insert into board db. */
		prop_dictionary_set_keysym(params->mach_data, key, board);
	}
	prop_object_iterator_release(iter);
	prop_object_release(overlay);
}

/*
 * evb_db_load_overlays --
 *	Load the overlays from the search path.
 */
static void
evb_db_load_overlays(ib_params *params)
{
	char overlay_pathbuf[PATH_MAX+1];
	const char *overlay_path;
	char **paths;
	void *pathsbuf = NULL;
	FTS *fts;
	FTSENT *chp, *p;
	struct stat sb;

	paths = evb_uboot_pkg_paths(params, NULL, &pathsbuf);
	if (paths == NULL) {
		warnx("No u-boot search path?");
		return;
	}

	fts = fts_open(paths, FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOCHDIR, NULL);
	if (fts == NULL ||
	    (chp = fts_children(fts, 0)) == NULL) {
		warn("Unable to search u-boot path");
		if (fts != NULL)
			fts_close(fts);
		return;
	}

	chp = fts_children(fts, 0);

	while ((p = fts_read(fts)) != NULL) {
		if (p->fts_info != FTS_D)
			continue;
		overlay_path = make_path(overlay_pathbuf,
		    sizeof(overlay_pathbuf), "%s/installboot.plist",
		    p->fts_path);
		if (overlay_path == NULL)
			continue;
		if (stat(overlay_path, &sb) < 0)
			continue;
		evb_db_load_overlay(params, overlay_path, p->fts_path);
	}

	fts_close(fts);
}

/*
 * evb_db_load_base --
 *	Load the base board db.
 */
static bool
evb_db_load_base(ib_params *params)
{
	char buf[PATH_MAX+1];
	const char *path;
	prop_dictionary_t board_db;
	struct stat sb;

	path = evb_db_base_path(params, buf, sizeof(buf));
	if (path == NULL)
		return false;

	if (params->flags & IB_VERBOSE)
		printf("Loading '%s'.\n", path);

	if (stat(path, &sb) < 0) {
		if (errno != ENOENT) {
			warn("'%s'", path);
			return false;
		}
		board_db = prop_dictionary_create();
		assert(board_db != NULL);
	} else {
		board_db = prop_dictionary_internalize_from_file(path);
		if (board_db == NULL) {
			warnx("unable to parse board db '%s'", path);
			return false;
		}
	}

	if (prop_dictionary_count(board_db) == 0) {
		/*
		 * Oh well, maybe we'll load some overlays.
		 */
		goto done;
	}

	/*
	 * Validate all of the board objects and remove any bad ones.
	 */
	prop_array_t all_board_keys = prop_dictionary_all_keys(board_db);
	prop_object_iterator_t iter = prop_array_iterator(all_board_keys);
	prop_dictionary_keysym_t key;
	prop_dictionary_t board;
	while ((key = prop_object_iterator_next(iter)) != NULL) {
		board = prop_dictionary_get_keysym(board_db, key);
		assert(board != NULL);
		if (!validate_board_object(board, false)) {
			warnx("invalid board object in '%s': '%s'", path,
			    prop_dictionary_keysym_cstring_nocopy(key));
			prop_dictionary_remove_keysym(board_db, key);
		}
	}
	prop_object_iterator_release(iter);
	prop_object_release(all_board_keys);

 done:
	params->mach_data = board_db;
	return true;

 bad:
	if (board_db != NULL)
		prop_object_release(board_db);
	return false;
}

/*
 * evb_db_load --
 *	Load the board database.
 */
bool
evb_db_load(ib_params *params)
{
	if (!evb_db_load_base(params))
		return false;
	evb_db_load_overlays(params);

	return true;
}

/*
 * evb_db_get_board --
 *	Return the specified board object from the database.
 */
evb_board
evb_db_get_board(ib_params *params, const char *name)
{
	return prop_dictionary_get(params->mach_data, name);
}

/*
 * evb_board_get_description --
 *	Return the description for the specified board.
 */
const char *
evb_board_get_description(ib_params *params, evb_board board)
{
	prop_string_t string;

	string = prop_dictionary_get(board, board_description_key);
	return prop_string_cstring_nocopy(string);
}

/*
 * evb_board_get_uboot_pkg --
 *	Return the u-boot package name for the specified board.
 */
const char *
evb_board_get_uboot_pkg(ib_params *params, evb_board board)
{
	prop_string_t string;

	string = prop_dictionary_get(board, board_u_boot_pkg_key);
	if (string == NULL)
		return NULL;
	return prop_string_cstring_nocopy(string);
}

/*
 * evb_board_get_uboot_path --
 *	Return the u-boot installed package path for the specified board.
 */
const char *
evb_board_get_uboot_path(ib_params *params, evb_board board)
{
	prop_string_t string;

	string = prop_dictionary_get(board, board_u_boot_path_key);
	if (string == NULL)
		return NULL;
	return prop_string_cstring_nocopy(string);
}

/*
 * evb_board_get_uboot_install --
 *	Return the u-boot install object for the specified board,
 *	corresponding to the media specified by the user.
 */
evb_ubinstall
evb_board_get_uboot_install(ib_params *params, evb_board board)
{
	evb_ubinstall install;

	install = prop_dictionary_get(board, board_u_boot_install_key);

	if (!(params->flags & IB_MEDIA)) {
		if (install == NULL) {
			warnx("Must specify media=... for board '%s'",
			    params->board);
			goto list_media;
		}
		return install;
	}

	/* media=... was specified by the user. */

	if (install) {
		warnx("media=... is not a valid option for board '%s'",
		    params->board);
		return NULL;
	}

	char install_key[128];
	int n = snprintf(install_key, sizeof(install_key), "%s-%s",
	    board_u_boot_install_key, params->media);
	if (n < 0 || (size_t)n >= sizeof(install_key))
		goto invalid_media;;
	install = prop_dictionary_get(board, install_key);
	if (install != NULL)
		return install;
 invalid_media:
	warnx("invalid media specification: '%s'", params->media);
 list_media:
	fprintf(stderr, "Valid media types:");
	prop_array_t array = evb_board_copy_uboot_media(params, board);
	assert(array != NULL);
	prop_object_iterator_t iter = prop_array_iterator(array);
	prop_string_t string;
	while ((string = prop_object_iterator_next(iter)) != NULL)
		fprintf(stderr, " %s", prop_string_cstring_nocopy(string));
	fprintf(stderr, "\n");
	prop_object_iterator_release(iter);
	prop_object_release(array);

	return NULL;
}

/*
 * evb_board_copy_uboot_media --
 *	Return the valid media types for the given board as an array
 *	of strings.
 *
 *	Follows the create rule; caller is responsible for releasing
 *	the array.
 */
prop_array_t
evb_board_copy_uboot_media(ib_params *params, evb_board board)
{
	prop_array_t array = prop_array_create();
	prop_object_iterator_t iter = prop_dictionary_iterator(board);
	prop_string_t string;
	prop_dictionary_keysym_t key;
	const char *cp;

	assert(array != NULL);
	assert(iter != NULL);

	while ((key = prop_object_iterator_next(iter)) != NULL) {
		cp = prop_dictionary_keysym_cstring_nocopy(key);
		if (strcmp(cp, board_u_boot_install_key) == 0 ||
		    strncmp(cp, board_u_boot_install_key,
			    sizeof(board_u_boot_install_key) - 1) != 0)
			continue;
		string = prop_string_create_cstring(strrchr(cp, '-'));
		assert(string != NULL);
		prop_array_add(array, string);
		prop_object_release(string);
	}
	prop_object_iterator_release(iter);
	return array;
}

/*
 * evb_ubinstall_get_steps --
 *	Get the install steps for a given install object.
 */
evb_ubsteps
evb_ubinstall_get_steps(ib_params *params, evb_ubinstall install)
{
	return prop_array_iterator(install);
}

/*
 * evb_ubsteps_next_step --
 *	Return the next step in the install object.
 *
 *	N.B. The iterator is released upon termination.
 */
evb_ubstep
evb_ubsteps_next_step(ib_params *params, evb_ubsteps steps)
{
	prop_dictionary_t step = prop_object_iterator_next(steps);

	/* If we are out of steps, release the iterator. */
	if (step == NULL)
		prop_object_iterator_release(steps);
	
	return step;
}

/*
 * evb_ubstep_get_file_name --
 *	Returns the input file name for the step.
 */
const char *
evb_ubstep_get_file_name(ib_params *params, evb_ubstep step)
{
	prop_string_t string = prop_dictionary_get(step, step_file_name_key);
	return prop_string_cstring_nocopy(string);
}

/*
 * evb_ubstep_get_file_offset --
 *	Returns the input file offset for the step.
 */
uint64_t
evb_ubstep_get_file_offset(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step, step_file_offset_key);
	if (number != NULL)
		return prop_number_unsigned_integer_value(number);
	return 0;
}

/*
 * evb_ubstep_get_file_size --
 *	Returns the size of the input file to copy for this step, or
 *	zero if the remainder of the file should be copied.
 */
uint64_t
evb_ubstep_get_file_size(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step, step_file_size_key);
	if (number != NULL)
		return prop_number_unsigned_integer_value(number);
	return 0;
}

/*
 * evb_ubstep_get_image_offset --
 *	Returns the offset into the destination image / device to
 *	copy the input file.
 */
uint64_t
evb_ubstep_get_image_offset(ib_params *params, evb_ubstep step)
{
	prop_number_t number = prop_dictionary_get(step, step_image_offset_key);
	if (number != NULL)
		return prop_number_unsigned_integer_value(number);
	return 0;
}

/*
 * evb_ubstep_preserves_partial_block --
 *	Returns true if the step preserves a partial block.
 */
bool
evb_ubstep_preserves_partial_block(ib_params *params, evb_ubstep step)
{
	prop_bool_t val = prop_dictionary_get(step, step_preserve_key);
	if (val != NULL)
		return prop_bool_true(val);
	return false;
}

/*
 * evb_uboot_file_path --
 *	Build a file path from the u-boot base path in the board object
 *	and the file name in the step object.
 */
static const char *
evb_uboot_file_path(ib_params *params, evb_board board, evb_ubstep step,
    char *buf, size_t bufsize)
{
	const char *base_path = evb_board_get_uboot_path(params, board);
	const char *file_name = evb_ubstep_get_file_name(params, step);
	int ret;

	if (base_path == NULL || file_name == NULL)
		return NULL;

	return make_path(buf, bufsize, "%s/%s", base_path, file_name);
}

/*
 * evb_uboot_do_step --
 *	Given a evb_ubstep, do the deed.
 */
static int
evb_uboot_do_step(ib_params *params, const char *uboot_file, evb_ubstep step)
{
	struct stat sb;
	int ifd = -1;
	char *blockbuf;
	size_t thisblock;
	off_t curoffset;
	off_t remaining;
	bool rv = false;

	uint64_t file_size = evb_ubstep_get_file_size(params, step);
	uint64_t file_offset = evb_ubstep_get_image_offset(params, step);
	uint64_t image_offset = evb_ubstep_get_image_offset(params, step);

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

	if (file_size)
		remaining = (off_t)file_size;
	else
		remaining = sb.st_size - (off_t)file_offset;

	if (params->flags & IB_VERBOSE) {
		if (file_offset) {
			printf("Writing '%s' -- %lld @ %" PRIu64 " ==> %" PRIu64 "\n",
			    evb_ubstep_get_file_name(params, step),
			    (long long)remaining, file_offset, image_offset);
		} else {
			printf("Writing '%s' -- %lld ==> %" PRIu64 "\n",
			    evb_ubstep_get_file_name(params, step),
			    (long long)remaining, image_offset);
		}
	}

	if (lseek(ifd, (off_t)file_offset, SEEK_SET) < 0) {
		warn("lseek '%s' @ %" PRIu64, uboot_file,
		    file_offset);
		goto out;
	}

	for (curoffset = (off_t)image_offset; remaining != 0;
	     remaining -= thisblock, curoffset += params->sectorsize) {
		thisblock = params->sectorsize;
		if (thisblock > remaining)
			thisblock = (size_t)remaining;
		if ((thisblock % params->sectorsize) != 0) {
			memset(blockbuf, 0, params->sectorsize);
			if (evb_ubstep_preserves_partial_block(params, step)) {
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
	rv = true;

 out:
	if (ifd != -1 && close(ifd) == -1)
		warn("close '%s'", uboot_file);
	if (blockbuf)
		free(blockbuf);
	return rv;
}

int
evb_uboot_setboot(ib_params *params, evb_board board)
{
	char uboot_filebuf[PATH_MAX+1];
	const char *uboot_file;
	struct stat sb;
	off_t max_offset = 0;
	int i;

	evb_ubinstall install = evb_board_get_uboot_install(params, board);
	evb_ubsteps steps;
	evb_ubstep step;

	/*
	 * First, make sure the files are all there.  While we're
	 * at it, calculate the largest byte offset that we will
	 * be writing.
	 */
	steps = evb_ubinstall_get_steps(params, install);
	while ((step = evb_ubsteps_next_step(params, steps)) != NULL) {
		uint64_t file_offset = evb_ubstep_get_file_offset(params, step);
		uint64_t file_size = evb_ubstep_get_file_size(params, step);
		uint64_t image_offset =
		    evb_ubstep_get_image_offset(params, step);
		uboot_file = make_path(uboot_filebuf,
		    sizeof(uboot_filebuf), "%s/%s",
		    evb_board_get_uboot_path(params, board),
		    evb_ubstep_get_file_name(params, step));
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
		off_t this_max = (sb.st_size - file_offset) + image_offset;
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
	steps = evb_ubinstall_get_steps(params, install);
	while ((step = evb_ubsteps_next_step(params, steps)) != NULL) {
		uboot_file = make_path(uboot_filebuf,
		    sizeof(uboot_filebuf), "%s/%s",
		    evb_board_get_uboot_path(params, board),
		    evb_ubstep_get_file_name(params, step));
		if (uboot_file == NULL)
			return 0;
		if (!evb_uboot_do_step(params, uboot_file, step))
			return 0;
	}

	return 1;
}
