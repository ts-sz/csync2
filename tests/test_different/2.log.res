cmd x "Updated both" local
Reading symbols from shared library libmysqlclient.so
Running recursive check for /export/home/dennis/Projects/csync2/csync2/tests/test ...
Checking for deleted files /export/home/dennis/Projects/csync2/csync2/tests/test recursive.
mark other operation: MKDIR peer:/export/home/dennis/Projects/csync2/csync2/tests/test/local '-'.
mark other operation: MKDIR other:/export/home/dennis/Projects/csync2/csync2/tests/test/local '-'.
mark other operation: NEW peer:/export/home/dennis/Projects/csync2/csync2/tests/test/local/different '-'.
mark other operation: NEW other:/export/home/dennis/Projects/csync2/csync2/tests/test/local/different '-'.
Connecting to host peer (PLAIN) ...
CONN peer < CONFIG 

CONN peer < HELLO local

Updating 'peer:/export/home/dennis/Projects/csync2/csync2/tests/test/local' MKDIR ''
CONN peer < SIG %25test%25 user/group 1000 1000 dennis schafroth 16877 
Updating 'peer:/export/home/dennis/Projects/csync2/csync2/tests/test/local/different' NEW ''
CONN peer < SIG %25test%25/different user/group 1000 1000 dennis schafroth 33188 
CONN peer < PATCH %25test%25/different - 1000 1000 dennis schafroth 33188 
While syncing file: /export/home/dennis/Projects/csync2/csync2/tests/test/local/different
ERROR from peer: File is also marked dirty here! (/export/home/dennis/Projects/csync2/csync2/tests/test/peer/different)
CONN peer < SETTIME %25test%25 
CONN peer < BYE

Finished with 1 errors.
