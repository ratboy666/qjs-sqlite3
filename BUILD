# BUILD
#
# Build with tcc or gcc.

CC=gcc
#
$CC -g -DJS_SHARED_LIBRARY=1 -fPIC -O2 -c sqlite3.c
$CC -g -shared -o sqlite3.so sqlite3.o -lsqlite3
#
# ce: .mshell;
