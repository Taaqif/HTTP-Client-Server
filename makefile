# makefile for server and client

#Everything
assignment2: myhttpd myhttp

#Server
myhttpd: myhttpd.c 
	gcc myhttpd.c -o myhttpd
	
#Client
myhttp: myhttp.c 
	gcc myhttp.c -o myhttp
clean: 
	rm *.o
