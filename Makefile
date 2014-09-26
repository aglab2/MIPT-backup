all:
	gcc backup.c -o backup

clean:
	rm -rf *.o backup
