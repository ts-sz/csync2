cmd c "move B newdir/C => new newdir/C" local
Reading symbols from shared library libmysqlclient.so
Running recursive check for /export/home/dennis/Projects/csync2/csync2/test/test ...
Checking for deleted files /export/home/dennis/Projects/csync2/csync2/test/test recursive.
mark other operation: 'RM' 'peer:/export/home/dennis/Projects/csync2/csync2/test/test/local/B' '-'.
mark other: Old operation: NEW '/export/home/dennis/Projects/csync2/csync2/test/test/local/B' '(null)' 
mark operation NEW -> RM peer:/export/home/dennis/Projects/csync2/csync2/test/test/local/B deleted before syncing. Removing from dirty.
mark other operation: 'RM' 'other:/export/home/dennis/Projects/csync2/csync2/test/test/local/B' '-'.
mark other: Old operation: NEW '/export/home/dennis/Projects/csync2/csync2/test/test/local/B' '(null)' 
mark operation NEW -> RM other:/export/home/dennis/Projects/csync2/csync2/test/test/local/B deleted before syncing. Removing from dirty.
mark other operation: 'MOD_DIR' 'peer:/export/home/dennis/Projects/csync2/csync2/test/test/local' '-'.
mark other: Old operation: MKDIR '/export/home/dennis/Projects/csync2/csync2/test/test/local' '(null)' 
mark operation NEW -> MOD => NEW peer:/export/home/dennis/Projects/csync2/csync2/test/test/local (not synced) .
mark other operation: 'MOD_DIR' 'other:/export/home/dennis/Projects/csync2/csync2/test/test/local' '-'.
mark other: Old operation: MKDIR '/export/home/dennis/Projects/csync2/csync2/test/test/local' '(null)' 
mark operation NEW -> MOD => NEW other:/export/home/dennis/Projects/csync2/csync2/test/test/local (not synced) .
mark other operation: 'MKDIR' 'peer:/export/home/dennis/Projects/csync2/csync2/test/test/local/newdir' '-'.
mark other operation: 'MKDIR' 'other:/export/home/dennis/Projects/csync2/csync2/test/test/local/newdir' '-'.
mark other operation: 'NEW' 'peer:/export/home/dennis/Projects/csync2/csync2/test/test/local/newdir/C' '-'.
mark other operation: 'NEW' 'other:/export/home/dennis/Projects/csync2/csync2/test/test/local/newdir/C' '-'.
Finished succesfully.
