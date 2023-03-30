#include "recover.h"
#include "tree.h"

#define BUFSIZE 1024*16



void md5(FILE *f);
void sha1(FILE *f);
void read_backup(NODE *parent_node, char *start_dir, char *current_dir);
char *format_size(off_t);
void get_time_from_name(char *dest, char *abs_path_time); 
void copy_file(char *src, char *dest);
void dfs(char *current_dir);
int check_hash(char *src, char *dest);

char home_dir[4096];
char backup_dir[4096];
char filename[4096];
char backup_path[4096];
char parent_path[4096];
char hash_func[100];
char hash[100];
char new_name[1000];
char new_dest[4096] = {0};

char file_list[1000][4096];
int list_idx;

NODE *BACKUP_NODE;

/*
   argv[0] : home_dir
   argv[1] : backup_dir
   argv[2] : filename's full path
   argv[3] : backup_path
   argv[4] : parent_path 
   argv[5] : new_name
   argv[6] : reg or dir
   argv[7] : hash_func
 */
int main(int argc, char *argv[]) {

	strcpy(home_dir, argv[0]);

	strcpy(backup_dir, argv[1]);
	strcpy(filename, argv[2]);
	strcpy(backup_path, argv[3]);
	strcpy(parent_path, argv[4]);
	memset(new_name, 0, sizeof(1000));
	if (strcmp(argv[5], "") != 0) {
		strcpy(new_name, argv[5]);
	}
	strcpy(hash_func, argv[7]);

	BACKUP_NODE = (NODE *)calloc(1, sizeof(NODE));
	strcpy(BACKUP_NODE->abs_path, backup_dir);
	strcpy(BACKUP_NODE->backup_path, backup_dir);
	BACKUP_NODE->is_dir = 1;
	BACKUP_NODE->is_reg = 1;

	read_backup(BACKUP_NODE, backup_dir, backup_dir);

	if (strcmp(argv[6], "REG") == 0) {	// 파일 recover

		NODE *pNODE = find_node_by_abs_path(BACKUP_NODE, parent_path);
		int child_count = get_child_count(pNODE, backup_path);

		if (child_count == 0) {
			fprintf(stderr, "no \"%s\" in backup directory\n", argv[4]);
			exit(1);
		}

		// n 옵션 시 새 경로 구하기
		if (strlen(new_name) > 0) {
			char *last_token = strrchr(filename, '/');
			memcpy(new_dest, filename, strlen(filename) - strlen(last_token));
			strcat(new_dest, "/");
			strcat(new_dest, new_name);
		}

		NODE **childs = get_childs(pNODE, backup_path, child_count);

		if (child_count == 1) {  // 이름이 같은 파일이 없을 때
			if (strlen(new_name) > 0) {  // n 옵션 시
				if (access(filename, F_OK) == 0) {  // 원본 파일이 존재하면 해시 검사
					if (check_hash(childs[0]->abs_path_time, filename) == 1) {
						printf("\"%s\" backup recover to \"%s\"\n", 
								childs[0]->abs_path_time, new_dest);
						copy_file(childs[0]->abs_path_time, new_dest);
					} else {
						printf("\"%s\" and \"%s\" is same file\n", childs[0]->abs_path_time, filename);
					}
				} else {  // 원본 파일이 없는 경우
					copy_file(childs[0]->abs_path_time, new_dest);
					printf("\"%s\" backup recover to \"%s\"\n", 
							childs[0]->abs_path_time, new_dest);
				}
			} else {  // n 옵션이 아닌 경우
					  // if origin file exists, then compare hash
				if (access(filename, F_OK) == 0) {
					if (check_hash(childs[0]->abs_path_time, filename) == 1) {
						copy_file(childs[0]->abs_path_time, filename);
						printf("\"%s\" backup recover to \"%s\"\n", 
								childs[0]->abs_path_time, filename);
					} else {
						printf("\"%s\" and \"%s\" is same file\n", childs[0]->abs_path_time, filename);
					}
				} else {
					// no origin file, copy file.
					copy_file(childs[0]->abs_path_time, filename);
					printf("\"%s\" backup recover to \"%s\"\n", 
							childs[0]->abs_path_time, filename);
				}
			}
		} else {  // 이름이 같은 파일이 있을 때
			int file_num = 0;
			printf("backup file list of \"%s\"\n", filename);
			printf("0. exit\n");
			for (int i=0; i<child_count; i++) {
				char time[100] = {0};
				get_time_from_name(time, childs[i]->abs_path_time);
				printf("%d.%s\t\t%sbytes\n", i+1, time, format_size(childs[i]->size));
			}
			printf("Choose file to recover\n>> ");
			scanf("%d", &file_num);
			if (file_num > 0) {
				if (strlen(new_name) > 0) {
					if (access(filename, F_OK) == 0) {
						if (check_hash(childs[0]->abs_path_time, filename) == 1) {
							printf("\"%s\" backup recover to \"%s\"\n", 
									childs[file_num-1]->abs_path_time, new_dest);
							copy_file(childs[file_num-1]->abs_path_time, new_dest);
						} else {
							printf("\"%s\" and \"%s\" is same file\n", childs[0]->abs_path_time, filename);
						}
					} else {
						printf("\"%s\" backup recover to \"%s\"\n", 
								childs[file_num-1]->abs_path_time, new_dest);
						copy_file(childs[file_num-1]->abs_path_time, new_dest);
					}
				} else {
					if (access(filename, F_OK) == 0) {
						if (check_hash(childs[0]->abs_path_time, filename) == 1) {
							printf("\"%s\" backup recover to \"%s\"\n", 
									childs[file_num-1]->abs_path_time, filename);
							copy_file(childs[file_num-1]->abs_path_time, filename);
						} else {

							printf("\"%s\" and \"%s\" is same file\n", childs[0]->abs_path_time, filename);
						}
					} else {
						printf("\"%s\" backup recover to \"%s\"\n", 
								childs[file_num-1]->abs_path_time, filename);
						copy_file(childs[file_num-1]->abs_path_time, filename);
					}
				}
			}
		}
	} else {  // 디렉토리 recover
		char mkdir_path[4096] = {0};
		char *last_token = strrchr(argv[2], '/');
		memcpy(mkdir_path, argv[2], strlen(argv[2]) - strlen(last_token));
		strcat(mkdir_path, "/");
		strcat(mkdir_path, argv[5]);

		char *token;
		token = strtok(mkdir_path, "/");

		char temp[400] = {0};
		strcat(temp, "/");
		strcat(temp, token);
		strcat(temp, "/");

		while (token != NULL) {
			if (access(temp, F_OK) < 0)
				mkdir(temp, 0755);


			token = strtok(NULL, "/");
			if (token != NULL) {
				strcat(temp, token);
				strcat(temp, "/");
			}
		}

		dfs(backup_path);
	}
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

char* get_current_time() {
	static char time_buf[30];
	time_t currentTime = time(NULL);
	struct tm *local = localtime(&currentTime);
	sprintf(time_buf, "%02d%02d%02d%02d%02d%02d",
			local->tm_year - 100, local->tm_mon + 1, local->tm_mday,
			local->tm_hour, local->tm_min, local->tm_sec);
	return time_buf;
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

char *format_size(off_t n) {
	static char str[64];
	char   temp[64];
	sprintf(temp, "%ld", n);

	int mod = strlen(temp) % 3;
	int    idx = 0;

	for(int i=0; i<strlen(temp); i++) {
		if(i != 0 && i % 3 == mod) {
			str[idx++] = ',';
		}
		str[idx++] = temp[i];
	}

	str[idx] = 0;
	return str;
}

void get_time_from_name(char *dest, char *abs_path_time) {
	char *last_token = strrchr(abs_path_time, '_');

	memcpy(dest, last_token+1, strlen(last_token) - 1);
}

void copy_file(char *src, char *dest) {

	int fd_from;
	int fd_to;

	char read_buf[1024];
	int length;
	if ((fd_from = open(src, O_RDONLY)) < 0) {
		fprintf(stderr, "open err 1\n");
		exit(1);
	}


	if ((fd_to = open(dest, O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0) {
		fprintf(stderr, "open err 2\n");
		exit(1);
	}



	while ((length = read(fd_from, read_buf, 1024)) > 0) {
		write(fd_to, read_buf, length);
	}

	remove(src);
}


int check_hash(char *src, char *dest) {
	char from_md5[100] = {0};
	char from_sha1[100] = {0};
	char to_md5[100] = {0};
	char to_sha1[100] = {0};

	FILE *IN = fopen(src, "r");
	md5(IN);
	strcpy(from_md5, hash);
	memset(hash, 0, sizeof(hash));
	fclose(IN);

	FILE *IN2 = fopen(src, "r");
	sha1(IN2);
	strcpy(from_sha1, hash);
	memset(hash, 0, sizeof(hash));
	fclose(IN2);

	FILE *IN3 = fopen(dest, "r");
	md5(IN3);
	strcpy(to_md5, hash);
	memset(hash, 0, sizeof(hash));
	fclose(IN3);

	FILE *IN4 = fopen(dest, "r");
	sha1(IN4);
	strcpy(to_sha1, hash);
	memset(hash, 0, sizeof(hash));
	fclose(IN4);

	char src_without_backup[4096] = {0};
	strcpy(src_without_backup, home_dir);
	memcpy(src_without_backup + strlen(home_dir), src+ strlen(backup_dir), strlen(src) - strlen(backup_dir));
	memset(src_without_backup + strlen(dest), 0, strlen(src_without_backup) - strlen(dest));
	if (strcmp(src_without_backup, dest) == 0) {
		if (strcmp(hash_func, "md5") == 0 && strcmp(from_md5, to_md5) == 0) 
			return -1;

		if (strcmp(hash_func, "sha1") == 0 && strcmp(from_sha1, to_sha1) == 0) 
			return -1;
	}

	return 1;
}

void dfs(char *current_dir) {
	DIR *dir = opendir(current_dir);
	if (dir == NULL) {
		fprintf(stderr, "dir open error for %s\n", current_dir);
		exit(1);
	}	

	read_backup(BACKUP_NODE, backup_dir, backup_dir);

	// remove /backup in abs_path
	// and mkdir
	char temp[4096] = {0};
	strcpy(temp, home_dir);
	memcpy(temp + strlen(home_dir), current_dir + strlen(backup_dir), 
			strlen(current_dir) - strlen(backup_dir));

	char new_dir[4096] = {0};
	char *prev_dir = strrchr(filename, '/');

	if (strlen(new_name) > 0) {
		char *old_name = strstr(temp, prev_dir);
		memcpy(new_dir, temp, old_name - temp);
		strcat(new_dir, "/");
		strcat(new_dir, new_name);
		memcpy(new_dir + strlen(new_dir), old_name + strlen(prev_dir), strlen(temp) - strlen(old_name) - strlen(prev_dir));

		mkdir(new_dir, 0755);
	} else {
		mkdir(temp, 0755);
	}

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
			fprintf(stderr, "lstat error1 : %s\n", abs_path);
			exit(1);
		}

		if (S_ISDIR(buf.st_mode)) {
			dfs(abs_path);
		} else if (S_ISREG(buf.st_mode)) {


			char abs_path_without_time[4096] = {0};
			char *last_token = strrchr(abs_path, '_');
			memcpy(abs_path_without_time, abs_path, strlen(abs_path) - strlen(last_token));

			// find whether abs_path_without_time is in file list 
			int is_find = 0;
			for (int i=0; i<list_idx; i++) {
				if (strcmp(file_list[i], abs_path_without_time) == 0) {
					is_find = 1;
					break;
				}
			}

			if (is_find == 0) {
				// find parent node
				NODE *pNODE = find_node_by_abs_path(BACKUP_NODE, current_dir);


				char dest_filename[4096] = {0};
				strcpy(dest_filename, home_dir);
				memcpy(dest_filename + strlen(home_dir), abs_path_without_time + strlen(backup_dir), strlen(abs_path_without_time) - strlen(backup_dir));

				int child_count = get_child_count(pNODE, abs_path_without_time);

				// find parent's childs
				NODE **childs = get_childs(pNODE, abs_path_without_time, child_count);
				if (child_count == 1) {
					if (strlen(new_name) > 0) {
						char new_file[4096] = {0};
						char *old_name = strstr(dest_filename, prev_dir);
						memcpy(new_file, dest_filename, old_name - dest_filename);
						strcat(new_file, "/");
						strcat(new_file, new_name);
						memcpy(new_file + strlen(new_file), old_name + strlen(prev_dir), strlen(dest_filename) - strlen(old_name) - strlen(prev_dir));

						if (access(new_file, F_OK) == 0) {
							if (check_hash(childs[0]->abs_path_time, new_file) == 1) {
								printf("\"%s\"backup recover to \"%s\"\n",
										childs[0]->abs_path_time, new_file);
								copy_file(childs[0]->abs_path_time, new_file);
							} else {

								printf("\"%s\" and \"%s\" is same file\n", childs[0]->abs_path_time, new_file);
							}
						} else {
							printf("\"%s\"backup recover to \"%s\"\n",
									childs[0]->abs_path_time, new_file);
							copy_file(childs[0]->abs_path_time, new_file);

						}
					} else {
						if (access(dest_filename, F_OK) == 0) {
							if (check_hash(childs[0]->abs_path_time, dest_filename) == 1) {
								printf("\"%s\"backup recover to \"%s\"\n",
										childs[0]->abs_path_time, dest_filename);
								copy_file(childs[0]->abs_path_time, dest_filename);
							} else {
								printf("\"%s\" and \"%s\" is same file\n", childs[0]->abs_path_time, dest_filename);

							}
						} else {
							printf("\"%s\"backup recover to \"%s\"\n",
									childs[0]->abs_path_time, dest_filename);
							copy_file(childs[0]->abs_path_time, dest_filename);

						}
					}
				} else {
					int file_num = 0;
					printf("backup file list of \"%s\"\n", dest_filename);
					printf("0. exit\n");
					for (int i=0; i<child_count; i++) {
						char time[100] = {0};
						get_time_from_name(time, childs[i]->abs_path_time);
						printf("%d.%s\t\t%sbytes\n", i+1, time, format_size(childs[i]->size));
					}
					printf("choose file to recover\n>> ");
					scanf("%d", &file_num);
					if (file_num > 0) {
						if (strlen(new_name) > 0) {
							char new_file[4096] = {0};

							char *old_name = strstr(dest_filename, prev_dir);
							memcpy(new_file, dest_filename, old_name - dest_filename);
							strcat(new_file, "/");
							strcat(new_file, new_name);
							memcpy(new_file + strlen(new_file), old_name + strlen(prev_dir), strlen(dest_filename) - strlen(old_name) - strlen(prev_dir));


							if (access(new_file, F_OK) == 0) {
								if (check_hash(childs[0]->abs_path_time, new_file) == 1) {
									copy_file(childs[0]->abs_path_time, new_file);
									printf("\"%s\"backup recover to \"%s\"\n",
											childs[0]->abs_path_time, new_file);
								} else {
									printf("\"%s\" and \"%s\" is same file\n", childs[0]->abs_path_time, new_file);
								}
							} else {
								copy_file(childs[0]->abs_path_time, new_file);
								printf("\"%s\"backup recover to \"%s\"\n",
										childs[0]->abs_path_time, new_file);

							}
						} else {
							if (access(dest_filename, F_OK) == 0) {
								if (check_hash(childs[file_num-1]->abs_path_time, dest_filename) == 1) {
									printf("\"%s\"backup recover to \"%s\"\n",
											childs[file_num-1]->abs_path_time, dest_filename);

									copy_file(childs[file_num-1]->abs_path_time, dest_filename);
								} else {
									printf("\"%s\" and \"%s\" is same file\n", childs[file_num-1]->abs_path_time, dest_filename);
								}
							} else {
								printf("\"%s\"backup recover to \"%s\"\n",
										childs[file_num-1]->abs_path_time, dest_filename);
								copy_file(childs[file_num-1]->abs_path_time, dest_filename);

							}
						}
					}
				}
				// add abs_path to file_list
				strcpy(file_list[list_idx++], abs_path_without_time);


			} else {
				// already recovered 
				continue;
			}

		}
	}

}
