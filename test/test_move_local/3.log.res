cmd c "move Z B => new B " local
Running recursive check for /export/home/dennis/Projects/csync2/csync2/test/test ...
Checking recursive for modified files /export/home/dennis/Projects/csync2/csync2/test/test 
mark other operation: 'MOD_DIR' 'peer:/export/home/dennis/Projects/csync2/csync2/test/test/local' '-'.
mark other: Old operation: MKDIR '/export/home/dennis/Projects/csync2/csync2/test/test/local' '(null)'
mark operation NEW -> MOD => NEW peer:/export/home/dennis/Projects/csync2/csync2/test/test/local (not synced) .
mark other operation: 'MOD_DIR' 'other:/export/home/dennis/Projects/csync2/csync2/test/test/local' '-'.
mark other: Old operation: MKDIR '/export/home/dennis/Projects/csync2/csync2/test/test/local' '(null)'
mark operation NEW -> MOD => NEW other:/export/home/dennis/Projects/csync2/csync2/test/test/local (not synced) .
csync_check_file_same_dev_inode /export/home/dennis/Projects/csync2/csync2/test/test/local/B
check same file (64) /export/home/dennis/Projects/csync2/csync2/test/test/local/Z -> /export/home/dennis/Projects/csync2/csync2/test/test/local/B 
Found MOVE /export/home/dennis/Projects/csync2/csync2/test/test/local/Z -> /export/home/dennis/Projects/csync2/csync2/test/test/local/B 
mark other operation: 'MV' 'peer:/export/home/dennis/Projects/csync2/csync2/test/test/local/B' '/export/home/dennis/Projects/csync2/csync2/test/test/local/Z'.
mark other: Old operation: NEW '/export/home/dennis/Projects/csync2/csync2/test/test/local/Z' '(null)'
mark operation NEW->MV => NEW peer '/export/home/dennis/Projects/csync2/csync2/test/test/local/B' '/export/home/dennis/Projects/csync2/csync2/test/test/local/Z' '/export/home/dennis/Projects/csync2/csync2/test/test/local/Z'.
mark other operation: 'MV' 'other:/export/home/dennis/Projects/csync2/csync2/test/test/local/B' '/export/home/dennis/Projects/csync2/csync2/test/test/local/Z'.
mark other: Old operation: NEW '/export/home/dennis/Projects/csync2/csync2/test/test/local/Z' '(null)'
mark operation NEW->MV => NEW other '/export/home/dennis/Projects/csync2/csync2/test/test/local/B' '/export/home/dennis/Projects/csync2/csync2/test/test/local/Z' '/export/home/dennis/Projects/csync2/csync2/test/test/local/Z'.
Checking for deleted files /export/home/dennis/Projects/csync2/csync2/test/test recursive.
Finished succesfully.
