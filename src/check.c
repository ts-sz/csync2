/*
 *  csync2 - cluster synchronization tool, 2nd generation
 *  LINBIT Information Technologies GmbH <http://www.linbit.com>
 *  Copyright (C) 2004, 2005, 2006  Clifford Wolf <clifford@clifford.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "csync2.h"
#include "digest.h"
#include "db_api.h"
#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef __CYGWIN__

#include <w32api/windows.h>

/* This does only check the case of the last filename element. But that should
 * be OK for us now...
 */
int csync_cygwin_case_check(const char *filename)
{
	if (!strcmp(filename, "/cygdrive"))
		goto check_ok;
	if (!strncmp(filename, "/cygdrive/", 10) && strlen(filename) == 11)
		goto check_ok;

	char winfilename[MAX_PATH];
	cygwin_conv_to_win32_path(filename, winfilename);

	int winfilename_len = strlen(winfilename);
	int found_file_len;
	HANDLE found_file_handle;
	WIN32_FIND_DATA fd;

	/* See if we can find this file. */
	found_file_handle = FindFirstFile(winfilename, &fd);
	if (found_file_handle == INVALID_HANDLE_VALUE)
		goto check_failed;
	FindClose(found_file_handle);

	found_file_len = strlen(fd.cFileName);

	/* This should never happen. */
	if (found_file_len > winfilename_len)
		goto check_failed;

	if (strcmp(winfilename + winfilename_len - found_file_len, fd.cFileName))
		goto check_failed;

check_ok:
	csync_debug(3, "Cygwin/Win32 filename case check ok: %s (%s)\n", winfilename, filename);
	return 1;

check_failed:
	csync_debug(2, "Cygwin/Win32 filename case check failed: %s (%s)\n", winfilename, filename);
	return 0;
}

#endif /* __CYGWIN__ */

#define CHECK_NEW_RM 1
#define CHECK_NEW_MOD 1
#define CHECK_MV_RM  0
#define CHECK_RM_MV  0
#define CHECK_HARDLINK 0

const char *csync_mode_op_str(int st_mode, int op)
{
    const char* operation = "???";
    if (op == OP_RM)
	return "RM";
    if (S_ISREG(st_mode))
	if (op == OP_NEW)
	    operation = "NEW";
	else
	    operation = "MOD";
    else if (S_ISDIR(st_mode))
	if (op == OP_NEW)
	    operation = "MKDIR";
	else
	    operation = "MOD_DIR";
    else if (S_ISCHR(st_mode))
	operation = "MKCHR";
    else if (S_ISBLK(st_mode))
	operation = "MKBLK";
    else if (S_ISFIFO(st_mode))
	operation = "MKFIFO";
    else
	csync_debug(1, "Unknown mode op: %d %d\n", st_mode, op);
    return operation;
}

void csync_hint(const char *file, int recursive)
{
	SQL("Adding Hint",
		"INSERT INTO hint (filename, recursive) "
		"VALUES ('%s', %d)", db_encode(file), recursive);
}

int csync_same_file(const char *file1, const char *file2) {
  struct stat st1, st2;
  int rc1 = stat(file1, &st1); 
  int rc2 = stat(file2, &st2);
  if (rc1 == 0 && rc2 == 0 && st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev)
    return 1;
  return 0;
}
int csync_same_stat_file(struct stat *st1, const char *file2) {
  struct stat st2;
  int rc2 = stat(file2, &st2);
  if (rc2 == 0 && st1->st_ino == st2.st_ino && st1->st_dev == st2.st_dev)
    return 1;
  return 0;
}

int csync_same_stat(struct stat *st1, struct stat *st2) {
  if (st1->st_ino == st2->st_ino && st1->st_dev == st2->st_dev)
    return 1;
  return 0;
}

void csync_mark_other(const char *file, const char *thispeer, const char *peerfilter, 
		      int operation_org, const char *checktxt,
		      const char *dev, const char *ino, const char *other, int mode)
{
    BUF_P buffer = buffer_init();
    struct peer *pl = csync_find_peers(file, thispeer);
    int pl_idx;
    int operation = operation_org;
    csync_schedule_commands(file, thispeer == 0);

    if ( ! pl ) {
	csync_debug(2, "Not in one of my groups: %s (%s)\n",
		    file, thispeer ? thispeer : "NULL");
	return;
    }

    struct stat st_file;
    int rc_file = stat(file, &st_file);
    for (pl_idx=0; pl[pl_idx].peername; pl_idx++) {
	operation = operation_org;
	if (!peerfilter || !strcmp(peerfilter, pl[pl_idx].peername)) {
	    csync_debug(1, "mark other operation: '%s' '%s:%s' '%s'.\n",
			csync_mode_op_str(st_file.st_mode, operation),
			pl[pl_idx].peername, file, (other ? other : "-"));
	    if (operation && operation == OP_MOVE && other == NULL) {
		csync_debug(1, "mark other MV operation missing other %s %s \n", pl[pl_idx].peername, file);
		operation = OP_UNDEF;
	    }
	    short dirty = 1;
	    short clean_other = 1;
	    char *result_other = 0;
	    char *file_new = buffer_strdup(buffer, file);
	    /* We dont currently have a checktxt when marking files. */ 
	    /* Disable for now: files part of MV gets deleted  if a file is deleted after check and before update in same run, 
	       and thus leaking other side */
	    if (1 && checktxt) {
	
		SQL_BEGIN("Checking old opertion(s) on dirty",
			  "SELECT operation, filename, other, op from dirty where "
			  "(checktxt = '%s' OR filename = '%s') AND device = %s AND inode  = %s AND peername = '%s' ",
			  db_encode(checktxt),
			  db_encode(file),
			  db_encode(dev),
			  db_encode(ino),
			  db_encode(pl[pl_idx].peername)
		    )
		{
		    operation_t old_operation;
		    const char *filename;
		    const char *other;
		    old_operation = csync_operation(SQL_V(0));
		    filename = db_decode(SQL_V(1));
		    other    = db_decode(SQL_V(2));
		    const char *old_op_str = SQL_V(3);
		    int old_op   = (old_op_str ? atoi(old_op_str) : 0);
	    
		    csync_debug(1, "mark other: Old operation: %s '%s' '%s'\n",
				csync_mode_op_str(mode, old_op), filename, other);
		    if (CHECK_HARDLINK && !rc_file && csync_same_stat_file(&st_file, filename)) {
			csync_debug(1, "mark operation NEW HARDLINK %s:%s->%s .\n", pl[pl_idx].peername, file, filename);
			operation = OP_HARDLINK;
			result_other = buffer_strdup(buffer,filename);
			clean_other = 0;
		    }
		    // NEW/MK A -> RM A => remove from dirty, as it newer happened if it is same filename
		    else if (CHECK_NEW_RM && operation == OP_RM && old_operation == OP_NEW && !strcmp(file,filename)) {
			csync_debug(1, "mark operation NEW -> RM %s:%s deleted before syncing. Removing from dirty.\n",
				    pl[pl_idx].peername, file);
			dirty = 0;
			operation = OP_UNDEF;
		    }
		    // NEW/MK A -> MOD (still NEW)
		    else if (CHECK_NEW_MOD && operation == OP_MOD && old_op == OP_NEW) {
			csync_debug(1, "mark operation NEW -> MOD => NEW %s:%s (not synced) .\n",
				    pl[pl_idx].peername, file);
			operation = old_op;
			dirty = 1;
		    }
		    else if (CHECK_RM_MV && ((OP_RM == old_operation || OP_MOVE == old_operation || OP_UNDEF == old_operation)
					     && OP_NEW == operation) && strstr(checktxt, "type=dir") == 0) {
			// TODO verify logic
			result_other = buffer_strdup(buffer, filename);
			csync_debug(1, "mark operation RM -> NEW => MOVE %s '%s' '%s'.\n",
				    pl[pl_idx].peername, file, result_other);
			operation = OP_MOVE;
		    }
		    else if (CHECK_MV_RM && OP_MOVE == old_operation && OP_RM == operation) {
			operation = OP_RM;
			file_new = buffer_strdup(buffer, other);
			result_other = buffer_strdup(buffer, filename);
			clean_other = 1;
			csync_debug(1, "mark operation MV->RM %s '%s' '%s' '%s'.\n",
				    pl[pl_idx].peername, file, filename, other);
			other = 0;
		    }
		    // Run only once? 
		    break; 
		} SQL_FIN {
		} SQL_END;
	    }
	    SQL("Deleting old dirty file entries",
		"DELETE FROM dirty WHERE filename = '%s' AND peername = '%s'",
		db_encode(file_new),
		db_encode(pl[pl_idx].peername));
      
	    /* Delete other file dirty status if differs from file_new */
	    if (clean_other && result_other && strcmp(file_new, result_other)) {
		SQL("Deleting old dirty file entries",
		    "DELETE FROM dirty WHERE filename = '%s' AND peername = '%s'",
		    db_encode(result_other),
		    db_encode(pl[pl_idx].peername));
	    }
	    if (dirty)
		SQL("Marking File Dirty",
		    "INSERT INTO dirty (filename, forced, myname, peername, operation, checktxt, device, inode, other, op, mode) "
		    "VALUES ('%s', %s, '%s', '%s', '%s', '%s', %s, %s, %s, %d, %d)",
		    db_encode(file_new),
		    csync_new_force ? "1" : "0",
		    db_encode(pl[pl_idx].myname),
		    db_encode(pl[pl_idx].peername),
		    csync_mode_op_str(mode, operation),
		    db_encode(checktxt),
		    (dev ? dev : "NULL"),
		    (ino ? ino : "NULL"),
		    csync_db_escape_quote((result_other ? result_other : other)),
		    operation,
		    mode
		    );
	};
    };
    free(pl);
    buffer_destroy(buffer);
}

void csync_mark(const char *file, const char *thispeer, const char *peerfilter, 
		int operation, const char *checktxt,
		const char *dev, const char *ino, int mode)
{
  csync_mark_other(file, thispeer, peerfilter, operation, checktxt, dev, ino, 0, mode);
}


/* Return path that doesn't exist */ 
/* Pre-cond: a non-existing file   */
char *csync_check_path(char *filename) 
{
  struct stat st;
  int missing = 0;
  int index = strlen(filename); 
  for (; index > 0; index--) {
    if (filename[index-1] == '/') {
      filename[index-1] = 0;
      /* Check for existence */
      if (lstat_strict(filename, &st) == 0) {
	/* check file status */
	if (S_ISDIR(st.st_mode)) {
	    filename[index-1] = '/';	  
	    if (!missing)
	      return 0;
	    else
	      return filename;
	}
	/* This shouldn't happen. We have a non-directory */
	csync_debug(0, "Check for directory failed with non-directory %s: %d", filename, st.st_mode);
	return 0;
      }
      else
	missing = 1;
    }
  }
  /* Weird. We went all to the way to the root */
  return 0; 
}

/* return 0 if path does not contain any symlinks */
int csync_check_pure(const char *filename)
{
#ifdef __CYGWIN__
  // For some reason or another does this function __kills__
  // the performance when using large directories with cygwin.
  // And there are no symlinks in windows anyways..
  if (!csync_lowercyg_disable)
    return 0;
#endif
  struct stat sbuf;
  int dir_len = 0;
  int i;
  int same_len;

  /* single entry last query cache
   * to speed up checks from deep subdirs */
  static struct {
    /* store inclusive trailing slash for prefix match */
    char *path;
    /* strlen(path) */
    int len;
    /* cached return value */
    int has_symlink;
  } cached;

  for (i = 0; filename[i]; i++)
    if (filename[i] == '/')
      dir_len = i+1;
  
  if (dir_len <= 1) /* '/' a symlink? hardly. */
    return 0;
  
  /* identical prefix part */
  for (i = 0; i < dir_len && i < cached.len; i++)
    if (filename[i] != cached.path[i])
      break;
  
  /* backtrack to slash */
  for (--i; i >= 0 && cached.path[i] != '/'; --i)
    ;
  same_len = i+1;

  csync_debug(3, "check_pure: filename: '%s' %u, cached path: '%s' %u, %u.\n", filename, dir_len, cached.path, cached.len, same_len);
  /* exact match? */
  if (dir_len == same_len && same_len == cached.len)
    return cached.has_symlink;
  
  { /* new block for myfilename[] */
    char myfilename[dir_len+1];
    char *to_be_cached;
    int has_symlink = 0;
    memcpy(myfilename, filename, dir_len);
    myfilename[dir_len] = '\0';
    to_be_cached = strdup(myfilename);
    i = dir_len-1;
    while (i) {
      for (; i && myfilename[i] != '/'; --i)
	;
      
      if (i <= 1)
	break;
      
      if (i+1 == same_len) {
	if (same_len == cached.len) {
	  /* exact match */
	  has_symlink = cached.has_symlink;
	  break;
	} else if (!cached.has_symlink)
	  /* prefix of something 'pure' */
	  break;
      }

      myfilename[i]=0;
      if (lstat_strict(myfilename, &sbuf) || S_ISLNK(sbuf.st_mode)) {
	has_symlink = 1;
	break;
      }
    }
    if (to_be_cached) { /* strdup can fail. So what. */
      free(cached.path);
      cached.path = to_be_cached;
      cached.len = dir_len;
      cached.has_symlink = has_symlink;
    }
    return has_symlink;
  }
}

void csync_generate_recursive_sql(const char *file_encoded, int recursive, char **where_rec) {
  if ( recursive ) {
    if ( !strcmp(file_encoded, "/") )
      ASPRINTF(where_rec, "OR 1=1");
    else {
      ASPRINTF(where_rec, "OR filename > '%s/' and filename < '%s0'", 
	       file_encoded, file_encoded);
    }
  }
}

int csync_check_del(const char *file, int recursive, int init_run)
{
    char *where_rec = "";
    struct textlist *tl = 0, *t;
    struct stat st;
    const char *SELECT_SQL = "SELECT filename, checktxt, device, inode, mode from file ";
    csync_debug(1, "Checking for deleted files %s%s\n", file, (recursive ? " recursive." : "."));
    const char *file_encoded = db_encode(file);
    csync_debug(3,"file %s encoded %s \n", file, file_encoded);

    csync_generate_recursive_sql(file_encoded, recursive, &where_rec);

    SQL_BEGIN("Checking for removed files",
	      "%s where (filename = '%s' %s) ORDER BY filename", 
	      SELECT_SQL, db_encode(file), where_rec)
    {
	const char *filename = db_decode(SQL_V(0));
	const char *checktxt = db_decode(SQL_V(1));
	const char *device   = db_decode(SQL_V(2));
	const char *inode    = db_decode(SQL_V(3));
	int mode    = (SQL_V(4) ? atoi(SQL_V(4)) : 0);
      
	if (!csync_match_file(filename))
	    continue;

	// Not found
	if ( lstat_strict(filename, &st) != 0 || csync_check_pure(filename))
	    textlist_add4(&tl, filename, checktxt, device, inode, mode);
    } SQL_END;
    int count_deletes = 0;
    for (t = tl; t != 0; t = t->next) {
	if (!init_run) {
	    //csync_debug(0, "check_dirty (rm): before mark (all) \n");
	    struct stat stat;
	    csync_mark(t->value, 0, 0, OP_RM, t->value2, t->value3, t->value4, t->intvalue);
	    count_deletes++;
	}
	SQL("Delete file from DB. It isn't with us anymore.",
	    "delete from file WHERE filename = '%s'",
	    db_encode(t->value));
    }

    textlist_free(tl);
  
    if ( recursive )
    	free(where_rec);
    return count_deletes;
}

struct textlist *csync_mark_hardlinks(const char *filename_enc, struct stat *st, struct textlist *tl)
{
  struct textlist *t = tl; 
  while (t) {
    char *src  = t->value;
    switch (t->intvalue) {
    case OP_HARDLINK: {
      char *operation = "MKH";
      SQL("Update operation to move/hardlink",
    	  "INSERT into dirty (filename, operation, op, other) values ('%s', '%s', %d, '%s')",
		  db_encode(src), operation, OP_HARDLINK, filename_enc);
      break;
    }
    /*
    case OP_MOVE:
      SQL("Remove delete operation (move)",
	"DELETE from dirty where filename = '%s'", db_encode(src));
      break; 
    */
    }
    
    t = t->next;
  }
  textlist_free(tl);
  return 0;
}

struct textlist *csync_check_all_same_dev_inode()
{
    struct textlist *tl = 0, *t;
    int count = 0;
    SQL_BEGIN("Check for same dev:inode for all dirty", 
	      "SELECT peername, device, inode, checktxt, count(*) from dirty group by 1,2,3,4 having count(*) > 1") {
	const char *db_peername  = db_decode(SQL_V(0));
	const char *db_device    = db_decode(SQL_V(1));
	const char *db_inode     = db_decode(SQL_V(2));
	const char *db_checktxt  = db_decode(SQL_V(3));
	textlist_add4(&tl, db_peername, db_device, db_inode, db_checktxt, 0);
    } SQL_FIN {
	csync_debug(2, "%d files with multiple dev:inode.\n", SQL_COUNT);
    } SQL_END; 
    return tl; 
}

struct textlist *csync_check_same_dev_inode(const char *peername, const char *filename, const char *digest, struct stat *st)
{
    struct textlist *tl = 0, *t;
    unsigned long long dev = (st->st_dev != 0 ? st->st_dev : st->st_rdev);
    unsigned long long ino = st->st_ino;
    char *peername_enc = strdup(db_encode(peername));
    char *filename_enc = strdup(db_encode(filename));
    const char *sqls[] = { " SELECT filename, checktxt, digest, operation FROM dirty "
			   " WHERE device = %lu and inode = %llu and filename != '%s' and peername = '%s' " ,
			   " SELECT filename, checktxt, digest, NULL FROM file " 
			   " WHERE device = %lu AND inode = %llu AND filename != '%s' ",
    }; 
    for (int index = 0; index < 2; index++) {
	SQL_BEGIN("Check for same dev:inode",
		  sqls[index],
		  dev, ino, filename_enc, peername_enc) {
	    const char *db_filename = db_decode(SQL_V(0));
	    const char *db_checktxt = db_decode(SQL_V(1));
	    const char *db_digest   = db_decode(SQL_V(2));
	    const char *db_operation= db_decode(SQL_V(3));
	
	    if (!digest || !db_digest || !strcmp(digest, db_digest)) {
		textlist_add_new3(&tl, db_filename, db_checktxt, db_operation);
	    }
	    else {
		csync_debug(1, "Different digest for %s %s ", digest, db_digest);
	    }
	} SQL_FIN {
	    csync_debug(2, "%d files with same dev:inode (%lu:%llu) as file: %s\n",
			SQL_COUNT, (unsigned long long) st->st_dev, (unsigned long long) st->st_ino, filename);
	} SQL_END;
    }
    free(filename_enc);
    free(peername_enc);
    return tl; 
}

struct textlist *csync_check_move(const char *peername, const char *filename, const char* checktxt, const char *digest, struct stat *st)
{
    struct textlist *t;
    struct textlist *db_tl = csync_check_same_dev_inode(peername, filename, digest, st);
    struct stat file_stat;
    int count = 0;
    for (t = db_tl; t != NULL; t = t->next) {
	const char *db_filename = t->value;
	const char *db_checktxt = t->value2;
	int rc = stat(db_filename, &file_stat);
	int db_version = csync_get_checktxt_version(db_checktxt);
	int i = 0; 
	if (rc) {
	    csync_debug(1, "Other file not found. Posible MOVE operation: %s\n", db_filename);
	    if (!(i = csync_cmpchecktxt(db_checktxt, checktxt))) {
		csync_debug(1, "OPERATION: MOVE %s to %s\n", filename, db_filename);
		t->intvalue = OP_MOVE;
	    }
	}
    }
    return db_tl;
}


struct textlist *csync_check_link_move(const char *peername, const char *filename, const char* checktxt, int operation, const char *digest,
				       struct stat *st, textlist_loop_t loop)
{
    struct textlist *t, *tl = NULL;
    struct textlist *db_tl = csync_check_same_dev_inode(peername, filename, digest, st);
    struct stat file_stat;
    int operation_new = operation == OP_NEW;
    int count = 0;
    for (t = db_tl; t != NULL; t = t->next) {
    	const char *db_filename = t->value;
    	const char *db_checktxt = t->value2;
    	const char *db_operation = t->value3;
	csync_debug(2, "check_link_move:  DB file: %s %s: %d\n", db_filename, db_operation, operation);
    	int rc = stat(db_filename, &file_stat);
    	int db_version = csync_get_checktxt_version(db_checktxt);
    	int i = 0;
	if (operation_new && db_operation && !strcmp(db_operation, "NEW")) {
	    csync_debug(1, "Unable to MOVE/LINK: both NEW\n");
	    continue;
	}
    	if (rc) {
	    csync_debug(1, "check_link_move: Other file not found. Possible MOVE operation: %s\n", db_filename);
	    if (!(i = csync_cmpchecktxt(db_checktxt, checktxt)))
	    {
		csync_debug(1, "OPERATION: MOVE %s to %s\n", db_filename, filename);
		textlist_add(&tl, db_filename, OP_MOVE);
	    }
    	} else { // LINK, not MV
	    db_checktxt = csync_genchecktxt_version(&file_stat, db_filename, SET_USER|SET_GROUP, db_version);
	    int i;
	    if (!(i = csync_cmpchecktxt(db_checktxt, checktxt))) {
		csync_debug(1, "csync_check_link_move: OPERATION MHARDLINK %s to %s\n", db_filename, filename);
		textlist_add(&tl, db_filename, OP_HARDLINK);
	    } else
	    { // LINK not verified
		csync_debug(1, "check_link: other file with same dev/inode, but different checktxt.");
		csync_debug(1, "File is different on peer (cktxt char #%d).\n", i);
		csync_debug(1, ">>> %s: %s\n", db_checktxt, db_filename);
		csync_debug(1, ">>> %s: %s\n", checktxt, filename);
		
		if (count > 1) {
		    csync_debug(0, "Multiple files with same inode: %s %s", filename, db_filename);
		    csync_debug(0, "Different checktxt %s %s", checktxt, db_checktxt);
		}
		count++;
	    }
    	}
    }
    textlist_free(db_tl);
    return tl; 
}

int csync_check_dir(const char* file, int recursive, int init_run, int version, int dirdump_this, int flags)
{
    struct dirent **namelist;
    int n = 0;
    int count_dirty = 0;
    csync_debug(2, "Checking %s%s* ..\n", file, !strcmp(file, "/") ? "" : "/");
    n = scandir(file, &namelist, 0, alphasort);
    if (n < 0) {
	csync_debug(0, "%s in scandir: %s (%s)\n", strerror(errno), file, file);
	csync_error_count++;
    } else {
	while(n--) {
	    on_cygwin_lowercase(namelist[n]->d_name);
	    if ( strcmp(namelist[n]->d_name, ".") &&
		 strcmp(namelist[n]->d_name, "..") ) {
		char fn[strlen(file)+
			strlen(namelist[n]->d_name)+2];
		sprintf(fn, "%s/%s",
			!strcmp(file, "/") ? "" : file,
			namelist[n]->d_name);
		char *operation;
		if (csync_check_mod(fn, recursive, 0, init_run, version, flags, &count_dirty))
		    dirdump_this = 1;
	    }
	    free(namelist[n]);
	}
	free(namelist);
    }
    if ( dirdump_this && csync_dump_dir_fd >= 0 ) {
	int written = 0, len = strlen(file)+1;
	while (written < len) {
	    int rc = write(csync_dump_dir_fd, file+written, len-written);
	    if (rc <= 0)
		csync_fatal("Error while writing to dump_dir_fd %d: %s\n",
			    csync_dump_dir_fd, strerror(errno));
	    written += rc;
	}
    }
    return count_dirty;
}

static int update_dev_inode(struct stat *file_stat, const char *dev, const char *ino)
{
    if (!dev)
	return 1;
    if (!ino)
	return 1;

    unsigned long long dev_no;
    unsigned long long ino_no;
    sscanf(dev, "%llu", &dev_no);
    sscanf(ino, "%llu", &ino_no);

    if (file_stat->st_dev != dev_no) {
	return 1;
    }
    if (file_stat->st_ino != ino_no) {	
	return 1;
    }
    return 0;
}

int csync_check_file_mod(const char *file, struct stat *file_stat, int init_run, int version)
{
    BUF_P buffer = buffer_init();
    int this_is_dirty = 0;
    int calc_digest = 0;
    int count = 0;

    char *checktxt = buffer_strdup(buffer, csync_genchecktxt_version(file_stat, file, SET_USER|SET_GROUP, version));
    // Assume that this isn't a upgrade and thus same version
    int is_upgrade = 0;
    int db_version = version;
    const char *encoded = db_encode(file);
    operation_t operation = 0;
    char *other = 0; 
    char *digest = NULL;
    SQL_BEGIN("Checking File",
	      "SELECT checktxt, inode, device, digest, mode, size, mtime FROM file WHERE "
	      "filename = '%s' ", encoded)
    {
    	db_version = csync_get_checktxt_version(SQL_V(0));

    	if (db_version < 1 || db_version > 2) {
    		csync_debug(0, "Error extracting version from checktxt: %s", SQL_V(0));
    	}
    	const char *checktxt_db = db_decode(SQL_V(0));
    	const char *checktxt_same_version = checktxt;
    	const char *inode    = SQL_V(1);
    	const char *device   = SQL_V(2);
    	const char *digest_p = SQL_V(3);
	digest = digest_p ? buffer_strdup(buffer, digest_p) : NULL;
	long mode;
	long size;
	long mtime;
	is_upgrade = SQL_V_long(4, &mode);
	is_upgrade = SQL_V_long(5, &size) || is_upgrade;
	is_upgrade = SQL_V_long(6, &mtime)|| is_upgrade;
    	int flag = 0;
    	if (strstr(checktxt_db, ":user=") != NULL)
    		flag |= SET_USER;
    	if (strstr(checktxt_db, ":group=") != NULL)
    		flag |= SET_GROUP;

    	if (update_dev_inode(file_stat, device, inode) ) {
    		csync_debug(0, "File %s has changed device:inode %s:%s -> %llu:%llu %o \n",
			    file, device, inode, file_stat->st_dev, file_stat->st_ino, file_stat->st_mode);
    		is_upgrade = 1;
    	}
    	if (!digest_p && strstr(checktxt, "type=reg")) {
    		calc_digest = 1;
    		is_upgrade = 1;
    	}
    	if (db_version != version || flag != (SET_USER|SET_GROUP)) {
    		checktxt_same_version = csync_genchecktxt_version(file_stat, file, flag, db_version);
    		if (csync_cmpchecktxt(checktxt, checktxt_same_version))
    			is_upgrade = 1;
    	}
    	if (csync_cmpchecktxt(checktxt_same_version, checktxt_db)) {
    		operation = OP_MOD;
    		csync_debug(2, "%s has changed: \n    %s \nDB: %s %s\n",
			    file, checktxt_same_version, checktxt_db, csync_operation_str(operation));
    		this_is_dirty = 1;
    	}
    } SQL_FIN {
		if ( SQL_COUNT == 0 ) {
			csync_debug(2, "New file: %s\n", file);
			operation = OP_NEW;
			if (S_ISREG(file_stat->st_mode)) {
				operation = OP_NEW;
				calc_digest = 1;
			}
			else if ( S_ISLNK(file_stat->st_mode) )
			{
				// TODO get max path
				int max = 1024;
				char target[max];
				int len = readlink(file, target, max-1);
				if (len > 0) {
					target[len] = 0;
					operation = OP_MOD;
					other = buffer_strdup(buffer, target);
				}
				else
					csync_debug(0, "Failed to read link on %s\n", file);
			}
			this_is_dirty = 1;
		}
    } SQL_END;

    if (calc_digest) {
    	int size = 2*DIGEST_MAX_SIZE+1;
    	digest = buffer_malloc(buffer, size);
    	int rc = dsync_digest_path_hex(file, "sha1", digest, size);
    	if (rc)
	    csync_debug(0, "Error generating digest: %s %d", digest, rc);
    }
    if (csync_compare_mode) {
	printf("%40s %s\n", digest ? digest : "-", file);
    }
    if ( (is_upgrade || this_is_dirty) && !csync_compare_mode ) {
	int has_links = (file_stat->st_nlink > 1 && S_ISREG(file_stat->st_mode));
	if (has_links) {
	    struct textlist *tl = NULL;
	    //csync_debug(2, "File %s has links: %d\n", file, file_stat->st_nlink);
	    // tl = csync_check_link(file, checktxt, file_stat, operation, csync_mark_hardlinks);
	    if (tl)
		textlist_free(tl);
	}

	unsigned long long dev = (file_stat->st_dev != 0 ? file_stat->st_dev : file_stat->st_rdev);
	const char *checktxt_encoded = db_encode(checktxt);
	if (is_upgrade) {
	    SQL("Update file entry",
		"UPDATE file set checktxt='%s', device=%lu, inode=%llu, digest=%s, mode=%lu, mtime=%lu, size=%lu where filename = '%s'",
		checktxt_encoded, dev, file_stat->st_ino, csync_db_quote(digest),
		file_stat->st_mode, file_stat->st_mtime, file_stat->st_size, encoded);
	}
	else {
	    SQL("Deleting old file entry", "DELETE FROM file WHERE filename = '%s'", encoded);
	    SQL("Adding or updating file entry",
		"INSERT INTO file (filename, checktxt, device, inode, digest, mode, size, mtime) "
		"VALUES ('%s', '%s', %lu, %llu, %s, %u, %lu, %lu)",
		encoded, 
		checktxt_encoded,
		dev,
		file_stat->st_ino,
		csync_db_quote(digest),
		file_stat->st_mode, 
		file_stat->st_size,
		file_stat->st_mtime
		);
	}
	if (!init_run && this_is_dirty) {
	    //      csync_debug(0, "check_dirty (mod): before mark (all) \n");
	    char dev_str[100];
	    char ino_str[100];
	    sprintf(dev_str, DEV_FORMAT, file_stat->st_dev);
	    sprintf(ino_str, INO_FORMAT, file_stat->st_ino);
	    csync_mark_other(file, 0, 0, operation,  checktxt_encoded, dev_str, ino_str, other, file_stat->st_mode);
	    count = 1;
	}
    }
    buffer_destroy(buffer);
    return count; 
}

int csync_check_mod(const char *file, int recursive, int ignnoent, int init_run, int version, int flags, int *count)
{
    int check_type = csync_match_file(file);
    int dirdump_this = 0, dirdump_parent = MATCH_NONE;
    int this_is_dirty = 0;
    struct stat st;
  
    if ( check_type>0 && lstat_strict(file, &st) != 0 ) {
	if ( ignnoent ) 
	    return 0;
	csync_debug(0, "check_mod: ERROR: Can't stat %s.\n", file);
	// TODO verify what to return, since caller of csync_check_mod is only checking for non-zero.
	// return ERROR;
	return  MATCH_NONE;
    }

    switch ( check_type ) 
    {
    case MATCH_NEXT:
	*count += csync_check_file_mod(file, &st, init_run, version);
	dirdump_this = 1;
	dirdump_parent = 1;
	//no break
    case MATCH_INTO:
	if ( !recursive ) 
	    break;
	if ( !S_ISDIR(st.st_mode) ) 
	    break;
	*count += csync_check_dir(file, recursive, init_run, version, dirdump_this, flags);
	break;
    default:
	csync_debug(2, "Don't check at all: %s\n", file);
	break;
    }
    return dirdump_parent;
}

/* 
   check for dirty files, updates the DB and returns number of dirty found in this check (or total?) NOT IMPLEMENTED
 */
int csync_check_recursive(const char *filename, int recursive, int init_run, int version, int flags)
{
#if __CYGWIN__
    if (!strcmp(filename, "/")) {
	filename = "/cygdrive";
    }
#endif
    csync_debug(1, "Running%s check for %s ...\n", recursive ? " recursive" : "", filename);
	
    // TODO How about swapping deletes and updates?
    int count_dirty = 0;
    if (!csync_compare_mode)
	count_dirty += csync_check_del(filename, recursive, init_run);
	
    csync_check_mod(filename, recursive, 1, init_run, version, flags, &count_dirty);

    const char *file_encoded = db_encode(filename); 

    return count_dirty; 
}


void csync_combined_operation(const char *peername, const char *dev, const char *inode, const char *checktxt) {

}

void csync_check(const char *filename, int recursive, int init_run, int version, int flags)
{
    int hasDirty = csync_check_recursive(filename, recursive, init_run, version, flags);
    struct textlist *dub_entries = csync_check_all_same_dev_inode(); 
    struct textlist *ptr = dub_entries;
    while (ptr != NULL) {
	csync_combined_operation(ptr->value, ptr->value2, ptr->value3, ptr->value4); 
	ptr = ptr->next;
    }
    textlist_free(dub_entries);
}


int csync_check_single(const char *filename, int init_run, int version)
{
  return csync_check_recursive(filename, 0, init_run, version, 0);
}

