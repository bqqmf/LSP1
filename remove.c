#include "remove.h"
#include "tree.h"

#define BUFSIZE 1024*16

void md5(FILE *f);
void sha1(FILE *f);
void read_backup(NODE *parent_node, char *start_dir, char *current_dir);
char *format_size(off_t);
void get_time_from_name(char *dest, char *abs_path_time); 
int check_hash(char *src, char *dest);
void remove_dir_c(char *current_dir, char *start_dir);
void remove_dir_a(char *current_dir);


char home_dir[4096];
char backup_dir[4096];
char filename[4096];
char backup_path[4096];
char parent_path[4096];
char hash_func[100];
char hash[100];
char input[100];
char option[10];
int regfile_count;
int subdir_count;



NODE *BACKUP_NODE;
/*
   argv[0] : home_dir
   argv[1] : backup_dir
   argv[2] : filename's full path
   argv[3] : backup_path
   argv[4] : parent_path
   argv[5] : reg or dir
   argv[6] : hash_func
   argv[7] : input
   argv[8] : option a or c
 */
int main(int argc, char *argv[])
{
	strcpy(home_dir, argv[0]);
	strcpy(backup_dir, argv[1]);
	strcpy(filename, argv[2]);
	strcpy(backup_path, argv[3]);
	strcpy(parent_path, argv[4]);
	strcpy(hash_func, argv[6]);
	strcpy(option, argv[8]);

	char *last_input = strrchr(argv[7], '/');
	if (last_input != NULL) 
		memcpy(input, last_input+1, strlen(last_input)-1);
	else 
		strcpy(input, argv[7]);

	BACKUP_NODE = (NODE *)calloc(1, sizeof(NODE));
	strcpy(BACKUP_NODE->abs_path, backup_dir);
	strcpy(BACKUP_NODE->backup_path, backup_dir);
	BACKUP_NODE->is_dir = 1;
	BACKUP_NODE->is_reg = 1;

	read_backup(BACKUP_NODE, backup_dir, backup_dir);

	if (strcmp(argv[5], "REG") == 0) {  // 파일 remove

		// find parent node
		NODE *pNODE = find_node_by_abs_path(BACKUP_NODE, parent_path);
		int child_count = get_child_count(pNODE, backup_path);

		if (child_count == 0) {
			fprintf(stderr, "no \"%s\" in backup directory\n", argv[7]);
			exit(1);
		}

		NODE **childs = get_childs(pNODE, backup_path, child_count);

		if (child_count == 1) {  // 이름이 같은 파일이 없을 때 
			if (remove(childs[0]->abs_path_time) < 0) {
				fprintf(stderr, "remove error for %s\n", childs[0]->abs_path_time);
				exit(1);
			} else
				printf("\"%s\" backup file removed\n", childs[0]->abs_path_time);
		} else {  // 이름이 같은 파일이 있을 때
			int file_num = 0;
			printf("backup file list of \"%s\"\n", filename);
			printf("0. exit\n");
			for (int i=0; i<child_count; i++) {
				char time[100] = {0};
				get_time_from_name(time, childs[i]->abs_path_time);
				printf("%d.%s\t\t%sbytes\n", i+1, time, format_size(childs[i]->size));
			}
			printf("Choose file to remove\n>> ");
			scanf("%d", &file_num);
			if (file_num > 0) {
				if (remove(childs[file_num-1]->abs_path_time) < 0) {
					fprintf(stderr, "remove error for %s\n", childs[file_num-1]->abs_path_time);
					exit(1);
				} else
					printf("\"%s\" backup file removed\n", childs[file_num-1]->abs_path_time);
			}
		}
	} else if (strcmp(argv[5], "DIR") == 0) {  // 디렉토리 remove

		// 옵션 c
		if (strcmp(argv[8], "c") == 0) {
			if (BACKUP_NODE->child->child == NULL) {
				printf("no file(s) in the backup\n");
				exit(1);
			}

			regfile_count = 0;
			subdir_count = 0;
			remove_dir_c(backup_path, backup_path);
			printf("remove backup cleard (%d regular files and %d subdirectories totally).\n",
					regfile_count, subdir_count);
		} else if (strcmp(argv[8], "a") == 0) {
			// 옵션 a
			remove_dir_a(backup_dir);
		} else {
			// 디렉토리의 하위 파일 삭제
			remove_dir_c(backup_path, backup_path);
		}
	}







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
	int idx = 0;

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

// remove -c 옵션
void remove_dir_c(char *current_dir, char *start_dir) {
	DIR *dir = opendir(current_dir);
	if (dir == NULL) {
		fprintf(stderr, "dir open error for %s\n", current_dir);
		exit(1);
	}

	// current_dir is backup path

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

		struct stat buf;

		char child_path[4096] = {0};
		strcpy(child_path, current_dir);
		strcat(child_path, "/");
		strcat(child_path, entry->d_name);

		if (lstat(child_path, &buf) < 0) {
			fprintf(stderr, "lstat error for %s\n", child_path);
			exit(1);
		}

		if (S_ISDIR(buf.st_mode)) {
			remove_dir_c(child_path, start_dir);
		} else if (S_ISREG(buf.st_mode)) {
			if (strcmp(option, "c") != 0) 
				printf("%s removed\n", child_path);
			regfile_count ++;

			remove(child_path);
		}

	}

	// rmdir(backup_path);
	if (strcmp(current_dir, start_dir) != 0) {
		rmdir(current_dir);
		subdir_count ++;
	}

	closedir(dir);
}

// remove -a 옵션 
void remove_dir_a(char *current_dir) {
	DIR *dir = opendir(current_dir);
	if (dir == NULL) {
		fprintf(stderr, "dir open error for %s\n", current_dir);
		exit(1);
	}

	// current_dir is backup path

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

		struct stat buf;

		char child_path[4096] = {0};
		strcpy(child_path, current_dir);
		strcat(child_path, "/");
		strcat(child_path, entry->d_name);

		if (lstat(child_path, &buf) < 0) {
			fprintf(stderr, "lstat error for %s\n", child_path);
			exit(1);
		}

		if (S_ISDIR(buf.st_mode)) {
			if (strcmp(entry->d_name, input) == 0) {
				remove_dir_c(child_path, child_path);
			}
			remove_dir_a(child_path);

		} else if (S_ISREG(buf.st_mode)) {
			char filename_without_time[256] = {0};
			char *last_token = strrchr(entry->d_name, '_');
			memcpy(filename_without_time, entry->d_name, strlen(entry->d_name) - strlen(last_token));

			if (strcmp(filename_without_time, input) == 0) {
				printf("%s removed\n", child_path);
				remove(child_path);
			}

		}

	}


	closedir(dir);
}

