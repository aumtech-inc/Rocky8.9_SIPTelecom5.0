CC=gcc -ggdb -Wall -Werror 

all:
	$(CC) -fPIC -shared -o liblua.so lualib.c 


test:
	$(CC) -DMAIN -o test lualib.c -llua  

