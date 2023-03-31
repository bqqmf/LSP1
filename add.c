#include "add.h"
#include "tree.h"

#define BUFSIZE 1024*16


char* get_current_time() {
	static char time_buf[30];
	time_t currentTime = time(NULL);
	struct tm *local = localtime(&currentTime);
	sprintf(time_buf, "%02d%02d%02d%02d%02d%02d",
			local->tm_year - 100, local->tm_mon + 1, local->tm_mday,
			local->tm_hour, local->tm_min, local->tm_sec);
	return time_buf;
}

void md5(FILE *f);
void sha1(FILE *f);
void dfs(char *current_dir);
void read_backup(NODE *parent_node, char *start_dir, char *current_dir);

/*
   if file add,
   args[0] = abs_path
   args[1] = backup_path 
   args[2] = filename
   args[3] = time

   if dir add,
   args[4] = start_dir
 */

char hash_func[100];
char hash[100];
char home_dir[4096];
char backup_dir[4096];

NODE *BACKUP_NODE;

int main(int argc, char *argv[]) {

	if (argc == 4) {  // 파일 add
		int fd_from;
		int fd_to;

		char buf[100];
		int length;

		char backup_file[4096] = {0};
		sprintf(backup_file, "%s_%s", argv[1], argv[3]);

		if ((fd_from = open(argv[0], O_RDONLY)) < 0) {
			fprintf(stderr, "open err 1\n");
			exit(1);
		}

		if ((fd_to = open(backup_file, O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0) {
			fprintf(stderr, "open err 2\n");
			exit(1);
		}

		while ((length = read(fd_from, buf, 100)) > 0) {
			write(fd_to, buf, length);
		}
	} else if (argc == 5) {  // 디렉토리 add
							 // argv[0] : dir's abs_path
							 // argv[1] : backup_dir
							 // argv[2] : home_dir
							 // argv[3] : hash_func 
							 // argv[4] : time
		strcpy(backup_dir, argv[1]);
		strcpy(home_dir, argv[2]);
		strcpy(hash_func, argv[3]);

		BACKUP_NODE = (NODE *)calloc(1, sizeof(NODE));
		strcpy(BACKUP_NODE->abs_path, backup_dir);
		strcpy(BACKUP_NODE->backup_path, backup_dir);
		BACKUP_NODE->is_dir = 1;
		BACKUP_NODE->is_reg = 1;

		read_backup(BACKUP_NODE, backup_dir, backup_dir);
		dfs(argv[0]);
	}

	return 0;
}

void dfs(char *current_dir) {
	DIR *dir = opendir(current_dir);
	if (dir == NULL) {
		fprintf(stderr, "dir open error for %s\n", current_dir);
		exit(1);
	}

	int sub_file_count = 0;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

		struct stat buf;
		char abs_path[4096] = {0};
		sprintf(abs_path, "%s/%s", current_dir, entry->d_name);

		char backup_path[4096] = {0};
		strcpy(backup_path, backup_dir);
		memcpy(backup_path + strlen(backup_dir), abs_path + strlen(home_dir), 
				strlen(abs_path) - strlen(home_dir));

		if (lstat(abs_path, &buf) < 0) {
			fprintf(stderr, "lstat error\n");
			exit(1);
		}


		if (S_ISDIR(buf.st_mode)) {
			struct stat dir_buf = {0};
			if (lstat(backup_path, &dir_buf) == -1)
				mkdir(backup_path, 0755);
			dfs(abs_path);
		} else if (S_ISREG(buf.st_mode)) {

			sub_file_count++;

			int fd_from;
			int fd_to;

			char read_buf[1024];
			int length;

			char backup_file[4097] = {0};
			sprintf(backup_file, "%s_%s", backup_path, get_current_time());

			NODE *backup_node = find_node_by_abs_path(BACKUP_NODE->child, backup_path);
			if (backup_node != NULL) {

				if (strcmp(hash_func, "md5") == 0) {
					FILE *IN = fopen(abs_path, "r");
					md5(IN);
					if (strcmp(backup_node->hash_md5, hash) == 0) {
						printf("\"%s\" is already backuped\n", backup_node->abs_path_time);
						memset(hash, 0, sizeof(hash));
						continue;
					}
					memset(hash, 0, sizeof(hash));
					fclose(IN);
				}  else {
					FILE *IN2 = fopen(abs_path, "r");
					sha1(IN2);
					if (strcmp(backup_node->hash_sha1, hash) == 0) {
						printf("\"%s\" is already backuped\n", backup_node->abs_path_time);
						memset(hash, 0, sizeof(hash));
						continue;
					}
					memset(hash, 0, sizeof(hash));
					fclose(IN2);
				}
				if ((fd_from = open(abs_path, O_RDONLY)) < 0) {
					continue;
				}

				if ((fd_to = open(backup_file, O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0) {
					continue;
				}

				while ((length = read(fd_from, read_buf, 1024)) > 0) {
					write(fd_to, read_buf, length);
				}

				printf("\"%s\" backuped\n", backup_file);

			} else { 

				if ((fd_from = open(abs_path, O_RDONLY)) < 0) {
					fprintf(stderr, "open err 1\n");
					exit(1);
				}

				if ((fd_to = open(backup_file, O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0) {
					fprintf(stderr, "open err 2\n");
					exit(1);
				}

				while ((length = read(fd_from, read_buf, 1024)) > 0) {
					write(fd_to, read_buf, length);
				}

				printf("\"%s\" backuped\n", backup_file);
			}
		}
	}

	// 디렉토리를 만들었는데 하위 파일이 없다면 삭제
	if (sub_file_count == 0) {

		char no_child_dir[4096] = {0};
		strcpy(no_child_dir, backup_dir);
		strcat(no_child_dir, "/");
		memcpy(no_child_dir + strlen(backup_dir), current_dir + strlen(home_dir),
				strlen(current_dir) - strlen(home_dir));;

		rmdir(no_child_dir);
	}

	closedir(dir);
}

void read_backup(NODE *parent_node, char *start_dir, char *current_dir)
{
	DIR* dir = opendir(current_dir);
	if (dir == NULL) {
		fprintf(stderr, "dir open error for %s\n", current_dir);
		exit(1);
	}

	NODE *dir_node = (NODE *)calloc(1, sizeof(NODE));
	strcpy(dir_node->abs_path, current_dir);

	dir_node->is_dir = 1;
	strcpy(dir_node->time, get_current_time());
	strcpy(dir_node->parent_path, parent_node->abs_path);

	add_child_backup(parent_node, dir_node);

	struct dirent *entry;
	while ((entry=readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

		struct stat buf;
		char abs_path[4096] = {0};

		sprintf(abs_path, "%s/%s", current_dir, entry->d_name);

		if (lstat(abs_path, &buf) < 0) {
			fprintf(stderr, "lstat err\n");
			exit(1);
		}

		NODE *pNODE = find_node_by_abs_path(BACKUP_NODE->child, current_dir);


		if (S_ISDIR(buf.st_mode)) {
			read_backup(pNODE, start_dir, abs_path);
		} else {
			NODE *reg_node = (NODE *)calloc(1, sizeof(NODE));
			char abs_path_without_time[4096] = {0};
			char *last_token = strrchr(abs_path, '_');
			memcpy(abs_path_without_time, abs_path, strlen(abs_path) - strlen(last_token));


			strcpy(reg_node->abs_path, abs_path_without_time);
			strcpy(reg_node->abs_path_time, abs_path);
			strcpy(reg_node->parent_path, pNODE->abs_path);
			strcpy(reg_node->filename, entry->d_name);
			reg_node->is_reg = 1;
			reg_node->size = buf.st_size;

			FILE *IN = fopen(abs_path, "r");
			md5(IN);
			strcpy(reg_node->hash_md5, hash);
			memset(hash, 0, sizeof(hash));
			fclose(IN);

			FILE *IN2 = fopen(abs_path, "r");
			sha1(IN2);
			strcpy(reg_node->hash_sha1, hash);
			memset(hash, 0, sizeof(hash));
			fclose(IN2);

			add_child_backup(pNODE, reg_node);
		}
	}
	closedir(dir);
}
void md5(FILE *f)
{
	MD5_CTX c;
	unsigned char md[MD5_DIGEST_LENGTH];
	int fd;
	int i;
	static unsigned char buf1[BUFSIZE];

	fd=fileno(f);
	MD5_Init(&c);
	for (;;)
	{
		i=read(fd,buf1,BUFSIZE);
		if (i <= 0) break;
		MD5_Update(&c,buf1,(unsigned long)i);
	}
	MD5_Final(&(md[0]),&c);

	char temp[10] = {0};
	for (i=0; i<MD5_DIGEST_LENGTH; i++) {
		sprintf(temp, "%02x",md[i]);
		strcat(hash, temp);
	}
}
void sha1(FILE *f)
{
	SHA_CTX c;
	unsigned char md[SHA_DIGEST_LENGTH];
	int fd;
	int i;
	unsigned char buf2[BUFSIZE];

	fd=fileno(f);
	SHA1_Init(&c);
	for (;;)
	{
		i=read(fd,buf2,BUFSIZE);
		if (i <= 0) break;
		SHA1_Update(&c,buf2,(unsigned long)i);
	}
	SHA1_Final(&(md[0]),&c);
	char temp[10] = {0};
	for (i=0; i<SHA_DIGEST_LENGTH; i++) {
		sprintf(temp, "%02x",md[i]);
		strcat(hash, temp);
	}
}
