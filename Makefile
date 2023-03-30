all : ssu_backup add remove recover help

ssu_backup : ssu_backup.o tree.o
	gcc ssu_backup.o tree.o -o ssu_backup -g -lssl -lcrypto -DOPENSSL_API_COMPAT=10101

ssu_backup.o: ssu_backup.c
	gcc -c ssu_backup.c -g -lssl -lcrypto -DOPENSSL_API_COMPAT=10101

add : add.o tree.o
	gcc add.o tree.o -o add -g -lssl -lcrypto -DOPENSSL_API_COMPAT=10101
	
add.o : add.c
	gcc -c add.c -g -lssl -lcrypto -DOPENSSL_API_COMPAT=10101

tree.o : tree.c
	gcc -c tree.c -g -lssl -lcrypto -DOPENSSL_API_COMPAT=10101

remove : remove.o tree.o
	gcc remove.o tree.o -o remove -g -lssl -lcrypto -DOPENSSL_API_COMPAT=10101

remove.o : remove.c
	gcc -c remove.c -g -lssl -lcrypto -DOPENSSL_API_COMPAT=10101

recover : recover.o tree.o
	gcc recover.o tree.o -o recover -g -lssl -lcrypto -DOPENSSL_API_COMPAT=10101

recover.o : recover.c
	gcc -c recover.c -g -lssl -lcrypto -DOPENSSL_API_COMPAT=10101

help : help.c
	gcc help.c -o help -lssl -lcrypto -DOPENSSL_API_COMPAT=10101


clean :
	rm ssu_backup.o
	rm ssu_backup
	rm add
	rm help
