#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#include <unistd.h>

#include <err.h>
#include <errno.h>
#include <string.h>

#define DEBUG

#ifdef DEBUG
    #define PRINTD(fmt, args...) printf(fmt, ## args)
#else
    #define PRINTD(fmt, args...)
#endif

#define __STR(const_num) # const_num
#define STR(const_num) __STR(const_num)

#define PERROR(name, fmt, args...) \
    do{printf(fmt, ## args); usage(name);}while(0)

#define PWARN(fmt, args...) \
	do{printf(fmt, ## args);}while(0)

#define BUF_SIZE 4096

void usage(const char * name){
    assert (name);

    printf("Usage: %s <backup_dir> <out_dir>\n",
        name);

    exit(EXIT_FAILURE);
}

int cp(char * path_in, char * path_out){
	int fd_in = open(path_in, O_RDONLY);
	if (fd_in == -1) return 1;
	int fd_out = open(path_out, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd_out == -2) return 2;
	char buf[BUF_SIZE];
	int read_bytes = 0, wrote_bytes = 0;
	while ((read_bytes = read(fd_in, &buf, sizeof(buf))) > 0){
		wrote_bytes = write(fd_out, &buf, read_bytes);
			PRINTD("cp: %s --> %s: W/R = %d/%d\n", path_out, path_in, wrote_bytes, read_bytes);
				if (wrote_bytes != read_bytes) return wrote_bytes - read_bytes;
	}
	close(fd_in);
	close(fd_out);
	return 0;
}

int cmp(char * path1, char * path2){
	char buf1[BUF_SIZE];
	memset(buf1, 0, sizeof(buf1));
	char buf2[BUF_SIZE];
	memset(buf2, 0, sizeof(buf2));
	int fd1 = open(path1, O_RDONLY);
	if (fd1 == -1) return -1;
	int fd2 = open(path2, O_RDONLY);
	if (fd2 == -1) return -2;
	int read_bytes1, read_bytes2;

	while ((read_bytes1 = read(fd1, &buf1, sizeof(buf1))) > 0  &&
	       (read_bytes2 = read(fd2, &buf2, sizeof(buf2))) > 0){	
		PRINTD("cmp: %s <?> %s: R0/R2 = %d/%d\n", path1, path2, read_bytes1, read_bytes2);
		if (memcmp(buf1, buf2, BUF_SIZE) != 0 || read_bytes1 != read_bytes2)
			return 1;
	}
	close(fd1);
	close(fd2);
	return 0;
}

void listdir(const char *fname, const char *name, const char *out){
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name)) || !(entry = readdir(dir))){
		PWARN("Dir \"%s\" failed: %s\n\n", name, strerror(errno));
		return;
	}

	do{
		char path[FILENAME_MAX];
		int len = snprintf(path, sizeof(path)-1, "%s/%s", name, entry->d_name);
		path[len] = 0;
		char path_out[FILENAME_MAX];
		len = snprintf(path_out, sizeof(path_out)-1, "%s/%s", out, entry->d_name);
		path_out[len] = 0;
        	char path_gz[FILENAME_MAX];
		len = snprintf(path_gz, sizeof(path_gz)-1, "%s/%s.gz", out, entry->d_name);
 		path_gz[len] = 0;

		if (entry->d_type == DT_DIR) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue; //It's about this dir and updir, shouldn't work with them

			if (mkdir (path_out, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
				if (errno != EEXIST)
					PERROR(fname, "Dir \"%s\" create failed: %s\n\n", name, strerror(errno));
			PRINTD("Processing dir %s\n", path);
			listdir(fname, path, path_out);
		}else{
			PRINTD("Processing file %s\n", path);
			if (access(path_gz, F_OK)){
				//no gzip, create it
				pid_t pid = fork();
				if (pid < 0){
					assert(!"Fork failure!");
				}else if (pid == 0){
					if (cp(path, path_out) == 0){
						execlp("gzip", "gzip", path_out, NULL);
						assert(!"gzip exec failure!");
					}
				}else{
					wait(NULL); //TODO check out signal
				}
			}else{
				//gzip file found, copy + check	
				pid_t pid = fork();
				if (pid < 0){
					assert(!"Fork failure!");
				}if (pid == 0){
					pid_t pid2 = fork();
					if (pid2 < 0){
						assert(!"Fork failure!");	
					}else if (pid2 == 0){
						execlp("gzip", "gzip", "-d", "-k", "-f", path_gz, NULL);
						assert(!"gzip exec failure!");
					}else{
						wait(NULL); //TODO check out signal

						if (cmp(path, path_out) != 0){
							if (cp (path, path_out) == 0){
								execlp("gzip", "gzip", "-f", path_out, NULL);
								assert(!"gzip exec failure!\n");
							}
						}else{
							remove(path_out);
						}
					}
				}else{
					wait(NULL); //TODO check out signal
				}
			}
		}
	}while (entry = readdir(dir));
	closedir(dir);
}

int main(int argv, char * argc[]){
	if (argv != 3) PERROR(argc[0], "Wrong number of arguments!\n\n");

	mkdir (argc[2], S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	listdir(argc[0], argc[1], argc[2]);

	while (wait(NULL) != -1) {};
	return 0;
}

