/*
	Copyright (C) 2004 Stephen Bach
	This file is part of the viewglob package.

	viewglob is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	viewglob is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with viewglob; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "viewglob-error.h"
#include "glob-expand.h"
#include <sys/stat.h>
#include <string.h>

#if HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#  define dirent direct
#  define NAMLEN(dirent) (dirent)->d_namlen
#  if HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
#endif

static int   parse_args(int argc, char** argv);
static void  report_version(void);
static void  compile_data(int, char**);
static void  report(void);
static void  initiate(dev_t pwd_dev_id, ino_t pwd_inode);
static void  correlate(char*, char*, dev_t, ino_t);

static long   get_max_path(const char*);
static char*  vg_dirname(const char*);
static char*  normalize_path(const char* path, bool remove_trailing);
static bool   has_trailing_slash(const char* path);

static enum file_type  determine_type(const struct stat* file_stat);
static File*           make_new_file(char* name, enum file_type type);
static Directory*      make_new_dir(char* dir_name, dev_t dev_id, ino_t inode);
static bool            mark_files(Directory* dir, char* file_name);
static bool            have_dir(char* name, dev_t dev_id, ino_t inode, Directory** return_dir);


/* Directory comparisons are done by inode and device id. */
static bool compare_by_inode(char* name1, char* name2, dev_t dev_id1, ino_t inode1, dev_t dev_id2, ino_t inode2);

static char* pwd;
static size_t pwd_length;

static Directory* dirs = NULL;

#if DEBUG_ON
FILE* df;
#endif

int main(int argc, char* argv[]) {
	long max_path;
	int offset;

	set_program_name(argv[0]);

#if DEBUG_ON
	df = fopen("/tmp/out2.txt", "w");
#endif

	offset = parse_args(argc, argv);

	/* Get max path length. */
	max_path = get_max_path(".");
	if (max_path == -1) {
		viewglob_fatal("Could not determine maximum path length");
	}

	pwd = XMALLOC(char, max_path);
	pwd = getcwd(pwd, max_path);
	if (pwd == NULL) {
		viewglob_fatal("Could not determine working directory");
	}

	/* strlen(pwd) is used frequently, so might as well just do it once. */
	pwd_length = strlen(pwd);

	compile_data(argc - offset, argv + offset);
	report();

#if DEBUG_ON
	if (fclose(df) != 0)
		viewglob_warning("Could not close debug file");
#endif

	return EXIT_SUCCESS;
}


/* Figure out where glob-expand's options end and its expansion output begins.
   Also determine which directory matching function will be used. */
static int parse_args(int argc, char** argv) {
	int i, j;
	bool has_double_dash = false;
	for (i = 1; i < argc; i++) {
		if ( strcmp("--", *(argv + i)) == 0 ) {
			has_double_dash = true;
			break;
		}
	}

	if (has_double_dash) {
		for (j = 1; j < i; j++) {
			if ( (strcmp("-v", *(argv + j)) == 0) ||
			     (strcmp("-V", *(argv + j)) == 0))
				report_version();
		}
		i++;
	}
	else
		i = 1;

	return i;
}


static void report_version(void) {
	printf("glob-expand %s\n", GLOB_EXPAND_VERSION);
	printf("Released %s\n", GLOB_EXPAND_RELEASE_DATE);
	exit(EXIT_SUCCESS);
}


static void compile_data(int argc, char** argv) {
	char* new_dir_name;
	char* new_file_name;
	char* normal_path;
	int i;

	struct stat dir_stat;

	/* Always expand on pwd, whether it appears in the arguments or not. */
	if ( stat(pwd, &dir_stat) == 0 )
		initiate(dir_stat.st_dev, dir_stat.st_ino);
	else
		viewglob_fatal("Could not read pwd");

	/* Loop through the arguments. */
	/* The first word is skipped, because that's the name of the command.
	   We do this here rather than just not passing it from seer, because the command line could be
	   of the form "{abc,def} ghi", which expands here to "abc def ghi", so "abc" is the command name
	   rather than "{abc,def}". */
	for (i = 1; i < argc && argv[i] != NULL; i++) {

		normal_path = normalize_path(argv[i], true);
		new_dir_name = vg_dirname(normal_path);
		new_file_name = basename(normal_path);
		DEBUG((df,"normal_path: %s\n", normal_path));
		DEBUG((df,"new_dir_name: %s\n", new_dir_name));
		DEBUG((df,"new_file_name: %s\n", new_file_name));

		if ( stat(new_dir_name, &dir_stat) == 0 )
			correlate(new_dir_name, new_file_name, dir_stat.st_dev, dir_stat.st_ino);

		XFREE(new_file_name);    /* When the filename is used, a copy is made from the dirent entry. */
		                         /* new_dir_name, however, is used.  It is freed elsewhere if not. */
		XFREE(normal_path);      /* normal_path is never saved. */

		if (has_trailing_slash(argv[i])) {
			/* The file argument is being referenced as a directory.  We've already dealt with it as
			   a file, so now lets also see if it's a directory, and if so we'll print its contents
			   too. */
			normal_path = normalize_path(argv[i], false);
			new_dir_name = vg_dirname(normal_path);
			DEBUG((df,"\tnormal_path: %s\n", normal_path));
			DEBUG((df,"\tnew_dir_name: %s\n", new_dir_name));

			if ( stat(new_dir_name, &dir_stat) == 0 )
				correlate(new_dir_name, NULL, dir_stat.st_dev, dir_stat.st_ino);

			XFREE(normal_path);
		}
	}
}


static bool has_trailing_slash(const char* path) {
	int i;

	for (i = 0; *(path + i) != '\0'; i++)
		;

	if (i) {
		i--;
		return *(path + i) == '/';
	}
	else
		return false;
}


static void report(void) {
	Directory* dir_iter;
	File* file_iter;
	char types[8];
	char selections[3];

	types[FT_REGULAR] = 'r';
	types[FT_EXECUTABLE] = 'e';
	types[FT_DIRECTORY] = 'd';
	types[FT_BLOCKDEV] = 'b';
	types[FT_CHARDEV] = 'c';
	types[FT_FIFO] = 'f';
	types[FT_SOCKET] = 's';
	types[FT_SYMLINK] = 'y';

	selections[S_YES] = '*';
	selections[S_MAYBE] = '~';
	selections[S_NO] = '-';

	for (dir_iter = dirs; dir_iter; dir_iter = dir_iter->next_dir) {
		printf("%d %d %d %s\n", dir_iter->selected_count, dir_iter->file_count, dir_iter->hidden_count, dir_iter->name);
		
		for (file_iter = dir_iter->file_list; file_iter; file_iter = file_iter->next_file)
			printf("\t%c %c %s\n", selections[file_iter->selected], types[file_iter->type], file_iter->name);
	}

	printf("\n");   /* Always end output with a double \n. */
}


/* Scan through pwd. */
static void initiate(dev_t pwd_dev_id, ino_t pwd_inode) {
	dirs = make_new_dir(pwd, pwd_dev_id, pwd_inode);
}


/* Fit this new directory and file into the others that have been processed, if possible. */
static void correlate(char* dir_name, char* file_name, dev_t dev_id, ino_t dir_inode) {
	Directory* search_dir;

	if (have_dir(dir_name, dev_id, dir_inode, &search_dir)) {
		/* In this case search_dir is the located directory. */
		XFREE(dir_name);         /* Since the dir is already known, we don't need this. */
		if (file_name)
			mark_files(search_dir, file_name);
	}
	else {
		/* In this case search_dir is the last directory struct in the list, so add this
		   new dir to the end. */
		search_dir->next_dir = make_new_dir(dir_name, dev_id, dir_inode);
		if (file_name)
			mark_files(search_dir->next_dir, file_name);
	}
}


/* Check out if we've already got this directory in dirs.
   If so, return it.  If not, return the last dir in the dir_list. */
static bool have_dir(char* name, dev_t dev_id, ino_t inode, Directory** return_dir) {
	Directory* dir_iter;

	dir_iter = dirs;
	do {
		if (compare_by_inode(name, dir_iter->name, dev_id, inode, dir_iter->dev_id, dir_iter->inode)) {
			*return_dir = dir_iter;
			return true;
		}
	} while ( (dir_iter->next_dir != NULL) && (dir_iter = dir_iter->next_dir) );
	/* ^^ Don't want to interate to NULL, thus the weird invariant. */

	*return_dir = dir_iter;  /* This is the last directory struct in the list. */
	return false;
}


static bool compare_by_inode(char* name1, char* name2, dev_t dev_id1, ino_t inode1, dev_t dev_id2, ino_t inode2) {
	return inode1 == inode2 && dev_id1 == dev_id2;
}


/* Try to match the given file_name against all file structs in dir. */
static bool mark_files(Directory* dir, char* file_name) {

	File* file_iter;


	for (file_iter = dir->file_list; file_iter; file_iter = file_iter->next_file) {

		/* Don't bother with the file if it's already selected. */
		if (file_iter->selected == S_YES)
			continue;

		/* Try to match only up to the length of file_name. */
		if (strncmp(file_name, file_iter->name, strlen(file_name)) == 0) {

			if (strcmp(file_name, file_iter->name) == 0) {
				/* Explicit match. */
				file_iter->selected = S_YES;
				dir->selected_count++;
			}
			else {
				/* Only a partial match. */
				if (file_iter->selected != S_YES) {
					/* Might be S_NO, might be S_MAYBE, the result is the same. */
					file_iter->selected = S_MAYBE;
				}
			}
		}
	}

	return true;
}


static File* make_new_file(char* name, enum file_type type) {
	File* new_file;

	new_file = XMALLOC(File, 1);
	new_file->name = name;
	new_file->selected = S_NO;
	new_file->type = type;
	new_file->next_file = NULL;
	return new_file;
}


static Directory* make_new_dir(char* dir_name, dev_t dev_id, ino_t inode) {
	DIR* dirp;
	struct dirent* entry;

	Directory* new_dir;
	int entry_count = 0, hidden_count = 0;

	char* file_name;
	char* full_path;
	File* file_iter = NULL;
	struct stat file_stat;
	enum file_type type;

	new_dir = XMALLOC(Directory, 1);
	new_dir->name = dir_name;
	new_dir->dev_id = dev_id;
	new_dir->inode = inode;
	new_dir->selected_count = 0;
	new_dir->next_dir = NULL;

	dirp = opendir(dir_name);
	if (dirp == NULL) {
		viewglob_error("Directory is inaccessible");
		new_dir->file_list = NULL;
		new_dir->file_count = 0;
		new_dir->hidden_count = 0;
		return new_dir;
	}

	/* Cycle through the files in the real directory, and add them to the new_dir struct. */
	while (errno = 0, (entry = readdir(dirp)) != NULL) {

		/* Make a copy of the name since the original data isn't reliable. */
		file_name = XMALLOC(char, NAMLEN(entry) + 1);
		strcpy(file_name, entry->d_name);

		/* Need to have the full path for stat. */
		full_path = XMALLOC(char, strlen(dir_name) + 1 + NAMLEN(entry) + 1);
		(void)strcpy(full_path, dir_name);
		(void)strcat(full_path, "/");
		(void)strcat(full_path, entry->d_name);

		/* Stat to determine the file type.
		   Using lstat so that symbolic links are detected instead of followed.  May wish
		   to switch at some point. */
		if ( lstat(full_path, &file_stat) == -1 ) {
			viewglob_error("Could not stat file");
			type = FT_REGULAR;   /* We don't want to just skip this; assume it's regular. */
		}
		else
			type = determine_type(&file_stat);

		if (file_iter) {
			file_iter->next_file = make_new_file(file_name, type);
			file_iter = file_iter->next_file;
		}
		else {     /* It's the first file. */
			new_dir->file_list = make_new_file(file_name, type);
			file_iter = new_dir->file_list;
		}

		XFREE(full_path);

		entry_count++;
		if ( *(entry->d_name) == '.' )
			hidden_count++;
	}

	new_dir->file_count = entry_count;
	new_dir->hidden_count = hidden_count;

	if ( closedir(dirp) == -1 )
		viewglob_warning("Could not close directory");

	return new_dir;

}


static enum file_type determine_type(const struct stat* file_stat) {
	if (S_ISREG(file_stat->st_mode)) {
		if ( (file_stat->st_mode & S_IXUSR) == S_IXUSR ||
		     (file_stat->st_mode & S_IXGRP) == S_IXGRP ||
			 (file_stat->st_mode & S_IXOTH) == S_IXOTH    )
			return FT_EXECUTABLE;
		else
			return FT_REGULAR;
	}
	else if (S_ISDIR(file_stat->st_mode))
		return FT_DIRECTORY;
	else if (S_ISLNK(file_stat->st_mode))
		return FT_SYMLINK;
	else if (S_ISBLK(file_stat->st_mode))
		return FT_BLOCKDEV;
	else if (S_ISCHR(file_stat->st_mode))
		return FT_CHARDEV;
	else if (S_ISFIFO(file_stat->st_mode))
		return FT_FIFO;
	else if (S_ISSOCK(file_stat->st_mode))
		return FT_SOCKET;
	else
		return FT_REGULAR;
}


/* Determine the maximum path length.
   Taken almost verbatim from Marc J. Rochkind's Advanced Unix Programming, 2nd ed. */
static long get_max_path(const char* path) {
	long max_path;

	errno = 0;
	max_path = pathconf(path, _PC_PATH_MAX);
	if (max_path == -1) {
		if (errno == 0)			/* No limit... */
			max_path = 4096;	/* ... So guess. */
		else
			return -1;
	}
	return max_path + 1;	/* Just in case the size doesn't include space
							   For the null byte. */
}


/* Takes a normalized path and returns the directory name.
   I didn't like the POSIX version of dirname. */
static char* vg_dirname(const char* path) {
	char* dirname;
	int slash_pos;
	size_t path_length;

	path_length = strlen(path);

	/* Find the last / in the path. */
	slash_pos = find_prev(path, path_length - 1, '/');
	DEBUG((df, "from slash_pos: %s\n", path + slash_pos));
	DEBUG((df, "full: %s (%d)\n", path, path_length));
	DEBUG((df, "pwd: %s (%d)\n", pwd, pwd_length));

	if (*path != '/') {
		/* It's a relative path, so need to append pwd. */

		if (slash_pos == -1) {
			/* The file is at pwd. */
			dirname = XMALLOC(char, pwd_length + 1);
			(void)strcpy(dirname, pwd);
		}
		else {
			/* Build an absolute path with pwd and the argument's path. */
			dirname = XMALLOC(char, pwd_length + 1 + slash_pos + 1);
			(void)strcpy(dirname, pwd);
			if (strcmp(dirname, "/") != 0) {
				/* (Kludge for directories at root) */
				(void)strcat(dirname, "/");
			}
			(void)strncat(dirname, path, slash_pos);
			*(dirname + pwd_length + 1 + slash_pos) = '\0';	/* Just in case. */
		}
	}
	else if (slash_pos == 0) {
		/* The file is at root (or is root). */
		dirname = XMALLOC(char, 2);
		(void)strcpy(dirname, "/");
	}
	else {
		/* It's an absolute path. */
		dirname = XMALLOC(char, slash_pos + 1);
		(void)strncpy(dirname, path, slash_pos);
		*(dirname + slash_pos) = '\0';
	}

	return dirname;
}


/* Removes repeated /'s and takes out the ending /, if present. */
static char* normalize_path(const char* path, bool remove_trailing) {
	char* norm;
	int i, norm_pos;
	bool slash_seen;
	size_t length;

	length = strlen(path);
	norm = XMALLOC(char, length + 1);	/* We'll need at most this much memory. */
	norm_pos = 0;

	slash_seen = false;
	for (i = 0; i < length; i++) {

		if (*(path + i) == '/') {
			if (slash_seen)
				continue;	/* Only copy the first slash. */
			else {
				slash_seen = true;
				*(norm + norm_pos) = '/';
				norm_pos++;
			}
		}
		else {
			slash_seen = false;
			*(norm + norm_pos) = *(path + i);
			norm_pos++;
		}
	}

	*(norm + norm_pos) = '\0';

	/* If remove_trailing == true, strip off a trailing / (if any). */
	if (remove_trailing && norm_pos > 1 && *(norm + norm_pos - 1) == '/')
		*(norm + norm_pos - 1) = '\0';

	return norm;
}

