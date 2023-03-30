#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <dirent.h>
#include <ftw.h>
#include <errno.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#include "tree.h"
#include "add.h"
#include "remove.h"

#define SCHOOL_ID 20192421
#define BUFSIZE 1024*16


void add_dir_tree(char *dir_name);
void read_dir(NODE *parent, char *start_dir, char *current_dir);
void read_backup(NODE *parent, char *start_dir, char *current_dir);
void replace_backup_path(char *dest, char *input);
void replace_abs_path(char *dest, char *input);

void md5(FILE *f);
void sha1(FILE *f);

void print_ssubackup_usage() 
{
	printf("Usage: ssu_backup <md5 | sha1>\n");
}

void print_add_usage() 
{
	printf("Usage : add <FILENAME> [OPTION]\n  -d : add directory recursive\n");
}

void print_remove_usage() 
{
	printf("Usage : remove <FILENAME> [OPTION]\n");
	printf("  -a : remove all file (recursive)\n");
	printf("  -c : clear backup directory\n");
}

void print_recover_usage() 
{
	printf("Usage : recover <FILENAME> [OPTION]\n");
	printf("  -d : recover directory recursive\n");
	printf("  -n <NEWNAME> : recover file with new name\n");
}

void print_help() 
{
	pid_t pid = fork();
	if (pid == -1) printf("there's an error while calling help\n");
	if (pid == 0) {
		char *args[] = {NULL };
		execv("./help", args);
	}
	while(waitpid(pid, NULL, WNOHANG) == 0) continue;
}

void check_exception(int argc, char **argv) 
{
	if ((argc != 2) || !(strcmp(argv[1], "md5") == 0 || strcmp(argv[1], "sha1") == 0)) {
		print_ssubackup_usage();
		exit(0);
	}
}
int get_dir_count(const char *input) 
{
	char *s = (char* )malloc(sizeof(char *) * strlen(input));
	strcpy(s, input);
	char *token;
	int count = 0;

	token = strtok(s, "/");
	while(token != NULL) {
		count ++;
		token = strtok(NULL, "/");
	}
	free(s);
	return count;
}
char** split_dir(const char *input, int count) 
{
	char **words = (char **)malloc(sizeof(char*) * count);
	for(int i=0; i<count; i++) {
		words[i] = (char *)malloc(sizeof(char) * 256);
	}
	char *s = (char* )malloc(sizeof(char *) * strlen(input));
	strcpy(s, input);
	char *token;
	int i = 0;

	token = strtok(s, "/");
	while(token != NULL) {
		strcpy(words[i++], token);
		token = strtok(NULL, "/");
	}
	free(s);
	return words;
}


int get_word_count(const char *input) 
{
	char *s = (char* )malloc(sizeof(char *) * strlen(input));
	strcpy(s, input);
	char *token;
	int count = 0;

	token = strtok(s, " ");
	while(token != NULL) {
		count ++;
		token = strtok(NULL, " ");
	}
	free(s);
	return count;
}

char** split_input(const char *input, int count) 
{
	char **words = (char **)malloc(sizeof(char*) * count);
	for(int i=0; i<count; i++) {
		words[i] = (char *)malloc(sizeof(char) * 256);
	}
	char *s = (char* )malloc(sizeof(char *) * strlen(input));
	strcpy(s, input);
	char *token;
	int i = 0;

	token = strtok(s, " ");
	while(token != NULL) {
		strcpy(words[i++], token);
		token = strtok(NULL, " ");
	}
	free(s);
	return words;
}

char** copy_double_pointer(char **words, int count) 
{
	char **temp = (char **)malloc(sizeof(char*) * count);
	for(int i=0; i<count; i++) {
		temp[i] = (char *)malloc(sizeof(char) * (strlen(words[i]) + 1));
		strcpy(temp[i], words[i]);
	}
	return temp;
}	

char* get_current_time() 
{
	static char time_buf[30];
	time_t currentTime = time(NULL);
	struct tm *local = localtime(&currentTime);
	sprintf(time_buf, "%02d%02d%02d%02d%02d%02d",
			local->tm_year - 100, local->tm_mon + 1, local->tm_mday,
			local->tm_hour, local->tm_min, local->tm_sec);
	return time_buf;
}


char *hash_func;
char hash[100];

char *home_dir;
char backup_dir[4096];
NODE *HEAD;
NODE *BACKUP_NODE;


int main(int argc, char* argv[]) 
{
	// 예외 검사
	check_exception(argc, argv);
	// 해시 방법 저장
	hash_func = argv[1];

	if ((home_dir = getenv("HOME")) == NULL) {
		home_dir = getpwuid(getuid())->pw_dir;
	}
	sprintf(backup_dir, "%s/backup", home_dir);


	if (access(backup_dir, F_OK) < 0) {
		mkdir(backup_dir, 0755);
	} 

	char *time = get_current_time();

	HEAD = (NODE *)calloc(1, sizeof(NODE));
	strcpy(HEAD->abs_path, backup_dir);
	strcpy(HEAD->backup_path, backup_dir);
	HEAD->is_dir = 1;
	HEAD->is_reg = 1;

	BACKUP_NODE = (NODE *)calloc(1, sizeof(NODE));
	strcpy(BACKUP_NODE->abs_path, backup_dir);
	strcpy(BACKUP_NODE->backup_path, backup_dir);
	BACKUP_NODE->is_dir = 1;
	BACKUP_NODE->is_reg = 1;

	while (1) {
		if (access(backup_dir, F_OK) < 0) {
			mkdir(backup_dir, 0755);
		} 

		char input[201] = {0};
		printf("%d> ", SCHOOL_ID);
		fgets(input, 200, stdin);
		input[strcspn(input, "\n")] = '\0';

		if (strlen(input) == 0) {
			continue;
		}

		int word_count = get_word_count(input);
		char **words = split_input(input, word_count);

		// 옵션들
		int option_d = 0;
		int option_n = 0;
		int option_a = 0;
		int option_c = 0;
		int err = 0;
		int param_opt = -1;
		opterr = 0;
		optind = 0;

		char **temp_words = copy_double_pointer(words, word_count);
		while((param_opt = getopt(word_count, temp_words, "dnca")) != -1) {
			switch(param_opt) {
				case 'd' :	
					option_d = 1; 
					break;
				case 'n' :	
					option_n = 1; 
					break;
				case 'a' :	
					option_a = 1; 
					break;
				case 'c' :	
					option_c = 1; 
					break;
				case '?' :
					err = 1;
					break;
			}
		}

		for (int i=0; i<word_count; i++)
			free(temp_words[i]);
		free(temp_words);

		// 백업 트리 초기화
		if (BACKUP_NODE->child != NULL) {
			reset_backup_tree(BACKUP_NODE->child);
			BACKUP_NODE->child = NULL;
		}

		// 백업 디렉토리 트리 만들기
		read_backup(BACKUP_NODE, backup_dir, backup_dir);

		const char const *command = words[0];

		if (strcmp(command, "add") == 0) {
			if (word_count < 2 || word_count > 3) {
				print_add_usage();
				continue;
			}

			if (strstr(words[1], "~") != NULL) {
				char temp[4096] = {0};
				char *p = strstr(words[1], "~");
				memcpy(temp, home_dir, strlen(home_dir));
				memcpy(temp + strlen(home_dir), p + 1, strlen(words[1]) - 1);
				free(words[1]);
				words[1] = (char *)calloc(1, strlen(temp)+1);
				strcpy(words[1], temp);
			}

			char path_length[10000] = {0};
			realpath(words[1], path_length);
			if (strlen(path_length) > 4096) {
				fprintf(stderr, "file path can't be longer than 4096\n");
				continue;
			}
			char abs_path[4096] = {0};
			strcpy(abs_path, path_length);

			if (strstr(abs_path, home_dir) == NULL) {
				printf("\"%s\" can't be backuped\n", words[1]);
				continue;
			}
			if (strstr(abs_path, backup_dir) != NULL) {
				fprintf(stderr, "\"%s\" can't be backuped\n", words[1]);
				continue;
			}
			if ((word_count == 2 || word_count == 3) && (access(abs_path, F_OK) < 0)) {
				print_add_usage();
				continue;
			}
			struct stat statbuf;
			lstat(words[1], &statbuf);

			if (access(abs_path, F_OK) < 0) {
				fprintf(stderr, "\"%s\" not exists\n", words[1]);
				continue;
			}

			if (access(abs_path, R_OK) < 0) {
				fprintf(stderr, "cannot access \"%s\"\n", abs_path);
				continue;
			}

			int is_reg = S_ISREG(statbuf.st_mode);
			int is_dir = S_ISDIR(statbuf.st_mode); 

			if (!(is_reg || is_dir)) {
				printf("only regular or directory file can backup\n");
				continue;
			}

			if (is_reg && word_count == 3) {
				print_add_usage();
				continue;
			}

			if (is_dir && word_count == 2) {
				fprintf(stderr, "\"%s\" is a directory file\n", words[1]);
				continue;
			}

			if (word_count == 3) {
				if (!option_d) {
					print_add_usage();
					continue;
				}
			}

			char backup_path[4096] = {0};
			strcpy(backup_path, backup_dir);
			memcpy(backup_path + strlen(backup_dir), abs_path + strlen(home_dir), strlen(abs_path) - strlen(home_dir));

			// 파일 add
			if (is_reg) {
				char *last_token = strrchr(abs_path, '/');
				char parent_dir_path[4096] = {0};
				memcpy(parent_dir_path, abs_path, strlen(abs_path) - strlen(last_token));

				struct stat buf;
				lstat(parent_dir_path, &buf);

				// 디렉토리 create 
				add_dir_tree(parent_dir_path);

				char filename[256] = {0};
				memcpy(filename, last_token + 1, strlen(last_token)-1);
				NODE *pNODE = find_node_by_abs_path(HEAD, parent_dir_path);
				NODE *cNODE = (NODE *)calloc(1, sizeof(NODE));

				strcpy(cNODE->abs_path, abs_path);
				strcpy(cNODE->backup_path, backup_path);
				strcpy(cNODE->parent_path, pNODE->backup_path);
				strcpy(cNODE->filename, filename);
				cNODE->is_reg = 1;
				strcpy(cNODE->time, get_current_time());
				strcpy(cNODE->abs_path_time, abs_path);
				strcat(cNODE->abs_path_time, "_");
				strcat(cNODE->abs_path_time, cNODE->time);

				// hash
				FILE *IN = fopen(abs_path, "r");
				md5(IN);
				strcpy(cNODE->hash_md5, hash);
				memset(hash, 0, sizeof(hash));
				fclose(IN);

				FILE *IN2 = fopen(abs_path, "r");
				sha1(IN2);
				strcpy(cNODE->hash_sha1, hash);
				memset(hash, 0, sizeof(hash));
				fclose(IN2);

				// 트리 업데이트
				if (BACKUP_NODE->child != NULL) {
					reset_backup_tree(BACKUP_NODE->child);
					BACKUP_NODE->child = NULL;
				}
				read_backup(BACKUP_NODE, backup_dir, backup_dir);

				// 트리에 노드 추가
				add_child(pNODE, cNODE);
			} else if (is_dir) {  // 디렉토리 add

				realpath(words[1], abs_path);

				if (strcmp(abs_path, home_dir) != 0) {
					add_dir_tree(abs_path);

					pid_t pid = fork();
					if (pid == 0) {
						char *args[6] = {0};
						/*
						   args[0] = start_dir 
						   args[1] = backup_dir
						   args[2] = home_dir
						   args[3] = hash_func
						   args[4] = time 
						 */
						args[0] = abs_path; 
						args[1] = backup_dir; 
						args[2] = home_dir; 
						args[3] = hash_func; 
						args[4] = time; 
						execv("./add", args);
					} 
					while(waitpid(pid, NULL, WNOHANG) == 0) continue;
				} else {
					pid_t pid = fork();
					if (pid == 0) {
						char *args[6] = {0};

						//   args[0] = start_dir 
						//   args[1] = backup_dir
						//   args[2] = home_dir
						//   args[3] = hash_func
						//   args[4] = time 

						args[0] = home_dir; 
						args[1] = backup_dir; 
						args[2] = home_dir; 
						args[3] = hash_func; 
						args[4] = time; 
						execv("./add", args);
					}
					while(waitpid(pid, NULL, WNOHANG) == 0) continue;
				}
			}
		} else if (strcmp(command, "remove") == 0) {

			if (strstr(words[1], "~") != NULL) {
				char temp[4096] = {0};
				char *p = strstr(words[1], "~");
				memcpy(temp, home_dir, strlen(home_dir));
				memcpy(temp + strlen(home_dir), p + 1, strlen(words[1]) - 1);
				free(words[1]);
				words[1] = (char *)calloc(1, strlen(temp)+1);
				strcpy(words[1], temp);
			}
			if (word_count < 2 || word_count > 3) {
				print_remove_usage();
				continue;
			}

			if (option_c && word_count != 2) {
				print_remove_usage();
				continue;
			}

			if (err || (option_a && option_c)) {
				print_remove_usage();
				continue;
			}

			char path_length[10000] = {0};
			replace_abs_path(path_length, words[1]);
			if (strlen(path_length) > 4096) {
				fprintf(stderr, "file path can't be longer than 4096\n");
				continue;
			}

			char abs_path[4096] = {0};
			strcpy(abs_path, path_length);

			if (strstr(abs_path, home_dir) == NULL) {
				printf("\"%s\" can't be backuped\n", words[1]);
				continue;
			}

			if (strstr(abs_path, backup_dir) != NULL) {
				printf("\"%s\" can't be backuped\n", words[1]);
				continue;
			}
			char backup_path[4096] = {0};
			replace_backup_path(backup_path, words[1]);


			NODE *file = find_node_by_abs_path(BACKUP_NODE, backup_path);

			int is_reg = 0;
			int is_dir = 0;

			if (file != NULL) {
				is_reg = file->is_reg;
				is_dir = file->is_dir;
				if (is_reg) {
					if (access(file->abs_path_time, R_OK) < 0) {
						fprintf(stderr, "cannot access to \"%s\"\n", words[1]);
						continue;
					}
				} else if (is_dir && !option_a) {
					fprintf(stderr, "remove directory need option -a\n");
					continue;
				}
			}

			if (file == NULL && !option_c && !option_a) {
				fprintf(stderr, "file not exists.\n");
				continue;
			}

			// 옵션 c
			if (option_c) {
				pid_t pid = fork();
				if (pid == 0) {
					char *args[10] = {0};
					/*
					   args[0] = home_dir 
					   args[1] = backup_dir
					   args[2] = filename's full path
					   args[3] = backup_path
					   args[4] = parent_path
					   args[5] = REG or DIR
					   args[6] = hash_func
					   args[7] = input 
					   args[8] = option - a or c
					 */
					args[0] = home_dir; 
					args[1] = backup_dir; 
					args[2] = abs_path; 
					args[3] = backup_dir; 
					args[4] = ""; 
					args[5] = "DIR";
					args[6] = hash_func;
					args[7] = words[1];
					args[8] = "c";

					execv("./remove", args);
				}
				while(waitpid(pid, NULL, WNOHANG) == 0) continue;
			} else if (option_a) {  // 옵션 a
				pid_t pid = fork();
				if (pid == 0) {
					char *args[10] = {0};
					/*
					   args[0] = home_dir 
					   args[1] = backup_dir
					   args[2] = filename's full path
					   args[3] = backup_path
					   args[4] = parent_path
					   args[5] = REG or DIR
					   args[6] = hash_func
					   args[7] = input 
					   args[8] = option - a or c
					 */
					args[0] = home_dir; 
					args[1] = backup_dir; 
					args[2] = abs_path; 
					args[3] = backup_path; 
					args[4] = ""; 
					args[5] = "DIR";
					args[6] = hash_func;
					args[7] = words[1];
					args[8] = "a";

					execv("./remove", args);
				}
				while(waitpid(pid, NULL, WNOHANG) == 0) continue;

			} else if (is_reg) {  // 파일 remove
				pid_t pid = fork();
				if (pid == 0) {
					char *args[10] = {0};
					/*
					   args[0] = home_dir 
					   args[1] = backup_dir
					   args[2] = filename's full path
					   args[3] = backup_path
					   args[4] = parent_path
					   args[5] = REG or DIR
					   args[6] = hash_func
					   args[7] = input 
					   args[8] = option - a or c
					 */
					args[0] = home_dir; 
					args[1] = backup_dir; 
					args[2] = abs_path; 
					args[3] = backup_path; 
					args[4] = file->parent_path; 
					args[5] = "REG";
					args[6] = hash_func;
					args[7] = words[1];
					args[8] = "";
					if (option_a == 1) args[8] = home_dir;
					else if (option_c == 1) args[8] = backup_dir;

					execv("./remove", args);
				}
				while(waitpid(pid, NULL, WNOHANG) == 0) continue;
			} else if (is_dir) {  // 디렉토리 remove
				pid_t pid = fork();
				if (pid == 0) {
					char *args[9] = {0};
					/*
					   args[0] = home_dir 
					   args[1] = backup_dir
					   args[2] = filename's full path
					   args[3] = backup_path
					   args[4] = parent_path
					   args[5] = REG or DIR
					   args[6] = hash_func
					   args[7] = input 
					   args[8] = option
					 */
					args[0] = home_dir; 
					args[1] = backup_dir; 
					args[2] = abs_path; 
					if (option_c || option_a) {
						args[3] = backup_dir;
						args[4] = "";
					} else {
						args[3] = backup_path; 
						args[4] = file->parent_path; 
					}
					args[5] = "DIR";
					args[6] = hash_func;
					args[7] = words[1];
					args[8] = "a";
					execv("./remove", args);
				}
				while(waitpid(pid, NULL, WNOHANG) == 0) continue;
			}
		} else if (strcmp(command, "recover") == 0) {
			if (word_count < 2 || word_count > 5) {
				print_recover_usage();
				continue;
			}

			char path_length[10000] = {0};
			replace_abs_path(path_length, words[1]);

			if (strlen(path_length) > 4096) {
				fprintf(stderr, "file path can't be longer than 4096\n");
				continue;
			}

			char abs_path[4096] = {0};
			strcpy(abs_path, path_length);

			char backup_path[4096] = {0};
			replace_backup_path(backup_path, words[1]);

			if (strstr(words[1], "-") != NULL) {
				print_recover_usage();
				continue;
			}
			if (strstr(abs_path, home_dir) == NULL) {
				fprintf(stderr, "\"%s\" can't be backuped\n", words[1]);
				continue;
			}
			if (strstr(abs_path, backup_dir) != NULL) {
				fprintf(stderr, "\"%s\" can't be backuped\n", words[1]);
				continue;
			}
			NODE *file = find_node_by_abs_path(BACKUP_NODE, backup_path);

			if (file == NULL) {
				printf("file not exists\n");
				continue;
			}

			if (file->is_reg && access(file->abs_path_time, R_OK) < 0) {
				print_recover_usage();
				continue;
			}

			if (file->is_dir && access(file->abs_path, R_OK) < 0) {
				print_recover_usage();
				continue;
			}

			if (word_count == 5 && strstr(words[4], "-") != NULL) {
				print_recover_usage();
				continue;
			}

			if (option_d) {
				if (!(word_count == 3 || word_count == 5)) {
					print_recover_usage();
					continue;
				}

				if (strcmp(words[2], "-d") != 0) {
					print_recover_usage();
					continue;
				}
			}

			if (option_n) {
				if (word_count < 4 || word_count > 5) {
					print_recover_usage();
					continue;
				}

				if (word_count == 4 && strcmp(words[2], "-n") != 0) {
					print_recover_usage();
					continue;
				} else if (word_count == 5 && strstr(words[3], "-n") == NULL) {
					print_recover_usage();
					continue;
				}

				// check new name
				if (word_count == 4 && strcmp(words[2], "-n") == 0) {
					if (strlen(words[3]) > 4096) {
						fprintf(stderr, "new name can't be longer than 4096\n");
						continue;
					}
					if (strstr(words[3], backup_dir) != NULL) {
						fprintf(stderr, "\"%s\" can't be backuped\n", words[3]);
						continue;
					}

				}
				if (word_count == 5 && strcmp(words[3], "-n") == 0) {
					if (strlen(words[4]) > 4096) {
						fprintf(stderr, "new name can't be longer than 4096\n");
						continue;
					}
					if (strstr(words[4], backup_dir) != NULL) {
						fprintf(stderr, "\"%s\" can't be backuped\n", words[3]);
						continue;
					}
				}

			}
			if (err) {
				print_recover_usage();
				continue;
			}

			int is_reg = file->is_reg;
			int is_dir = file->is_dir;

			if (is_dir && !option_d) {
				fprintf(stderr, "recover directory with -d\n");
				continue;
			}

			if (!(is_reg || is_dir)) {
				fprintf(stderr, "can't access file\n");
				continue;
			}

			if (is_reg) {  // 파일 recover
				char p_path[4096] = {0};
				strcpy(p_path, file->parent_path);
				pid_t pid = fork();
				if (pid == 0) {
					char *args[8] = {0};
					/*
					   args[0] = home_dir 
					   args[1] = backup_dir
					   args[2] = filename's full path
					   args[3] = backup_path
					   args[4] = parent_path
					   args[5] = new_name
					   args[6] = REG or DIR
					   args[7] = hash_func
					 */
					args[0] = home_dir; 
					args[1] = backup_dir; 
					args[2] = abs_path; 
					args[3] = backup_path; 
					args[4] = p_path; 
					if (word_count == 4) args[5] = words[3];
					else if (word_count == 5) args[5] = words[4];
					else args[5] = "";

					args[6] = "REG";
					args[7] = hash_func;
					execv("./recover", args);
				} 
				//while(waitpid(pid, NULL, WNOHANG) == 0) continue;
				waitpid(pid, NULL, 0);
			} if (is_dir) {  // 디렉토리 recover
				pid_t pid = fork();
				if (pid == 0) {
					char *args[7] = {0};
					/*
					   args[0] = home_dir 
					   args[1] = backup_dir
					   args[2] = filename's full path
					   args[3] = backup_path
					   args[4] = parent_path
					   args[5] = new_name 
					   args[6] = REG or DIR
					   args[7] = hash_func
					 */
					args[0] = home_dir; 
					args[1] = backup_dir; 
					args[2] = abs_path; 
					args[3] = backup_path; 
					args[4] = file->parent_path; 
					if (word_count == 4) args[5] = words[3];
					else if (word_count == 5) args[5] = words[4];
					else args[5] = "";

					args[6] = "DIR";
					args[7] = hash_func; 
					execv("./recover", args);
				}
				while(waitpid(pid, NULL, WNOHANG) == 0) continue;
			}
		} else if (strcmp(command, "ls") == 0) {
			pid_t pid = fork();
			if (pid == 0) {
				char **args = (char **)calloc(word_count - 1, sizeof(char *));
				for (int i=0; i<word_count-1; i++) {
					args[i] = (char *)calloc(1, 10);
					strcpy(args[i], words[i+1]);
				}
				execv("/bin/ls", args);
			}
			while(waitpid(pid, NULL, WNOHANG) == 0) continue;
		} else if (strcmp(command, "vi") == 0) {
			pid_t pid = fork();
			if (pid == 0) {
				char **args = (char **)calloc(word_count - 1, sizeof(char *));
				for (int i=0; i<word_count-1; i++) {
					args[i] = (char *)calloc(1, 10);
					strcpy(args[i], words[i+1]);
				}
				execv("/bin/vi", args);
			}
			while(waitpid(pid, NULL, WNOHANG) == 0) continue;
		} else if (strcmp(command, "vim") == 0) {
			pid_t pid = fork();
			if (pid == 0) {
				char **args = (char **)calloc(word_count - 1, sizeof(char *));
				for (int i=0; i<word_count-1; i++) {
					args[i] = (char *)calloc(1, 10);
					strcpy(args[i], words[i+1]);
				}
				execv("/bin/vim", args);
			}
			while(waitpid(pid, NULL, WNOHANG) == 0) continue;
		} else if (strcmp(command, "exit") == 0) {
			exit(0);
		} else if (strcmp(command, "help") == 0) print_help();
		else print_help();

		memset(input, 0, 200 * sizeof(char));

		for(int i=0; i<word_count; i++) {
			free(words[i]);
		}
		free(words);
	}
	return 0;
}


void add_dir_tree(char *dir_name)
{
	char temp[4096] = {0};
	memcpy(temp, dir_name + strlen(home_dir), strlen(dir_name) - strlen(home_dir));
	int count = get_dir_count(dir_name) - 2;
	char **dirs = split_dir(temp, count);

	char abs_path[4096] = {0};
	strcpy(abs_path, home_dir);
	char parent_backup_path[4096] = {0};
	char child_backup_path[4095] = {0};
	strcpy(parent_backup_path, backup_dir);
	strcpy(child_backup_path, backup_dir);

	for (int i=0; i<count; i++) {
		strcat(abs_path, "/");
		strcat(abs_path, dirs[i]);
		strcat(child_backup_path, "/");
		strcat(child_backup_path, dirs[i]);

		NODE *node = (NODE *)calloc(1, sizeof(NODE));
		strcpy(node->parent_path, parent_backup_path);
		strcat(parent_backup_path, "/");
		strcat(parent_backup_path, dirs[i]);

		strcpy(node->abs_path, abs_path);
		strcpy(node->backup_path, child_backup_path);
		node->is_dir = 1;
		strcpy(node->time, get_current_time());
		strcpy(node->abs_path_time, abs_path); 
		strcat(node->abs_path_time, "_");
		strcat(node->abs_path_time, node->time);

		// before add, update backup tree
		if (BACKUP_NODE->child != NULL) {
			reset_backup_tree(BACKUP_NODE->child);
			BACKUP_NODE->child = NULL;
		}
		read_backup(BACKUP_NODE, backup_dir, backup_dir);

		//find parent node by abs_path 
		NODE *pNODE = find_node_by_backup_path(HEAD, node->parent_path);
		add_child(pNODE, node);
	}

	for(int i=0; i<count; i++) {
		free(dirs[i]);
	}
	free(dirs);

}

// current_dir must abs_path or pwd
void read_dir(NODE *parent_node, char *start_dir, char *current_dir)
{
	DIR *dir = opendir(current_dir);
	if (dir == NULL) {
		fprintf(stderr, "dir open error for %s\n", current_dir);
		exit(1);
	}

	NODE *dir_node = (NODE *)calloc(1, sizeof(NODE));
	strcpy(dir_node->abs_path, current_dir);

	char dir_backup_path[4096] = {0};
	strcpy(dir_backup_path, backup_dir);
	memcpy(dir_backup_path + strlen(backup_dir), current_dir + strlen(home_dir), strlen(current_dir) - strlen(home_dir));
	strcpy(dir_node->backup_path, dir_backup_path);
	dir_node->is_dir = 1;
	strcpy(dir_node->time, get_current_time());
	strcpy(dir_node->parent_path, parent_node->backup_path);

	// before add, update backup tree
	if (BACKUP_NODE->child != NULL) {
		reset_backup_tree(BACKUP_NODE->child);
		BACKUP_NODE->child = NULL;
	}
	read_backup(BACKUP_NODE, backup_dir, backup_dir);
	add_child(parent_node, dir_node);

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

		NODE *pNODE = find_node_by_backup_path(HEAD->child, dir_backup_path);

		if (S_ISDIR(buf.st_mode)) {
			read_dir(pNODE, start_dir, abs_path);
		} else {
			NODE *reg_node = (NODE *)calloc(1, sizeof(NODE));
			strcpy(reg_node->abs_path, abs_path);
			strcpy(reg_node->backup_path, pNODE->backup_path);
			strcat(reg_node->backup_path, "/");
			strcat(reg_node->backup_path, entry->d_name);
			strcpy(reg_node->parent_path, pNODE->backup_path);
			strcpy(reg_node->filename, entry->d_name);
			reg_node->is_reg = 1;
			reg_node->size = buf.st_size;
			strcpy(reg_node->time, get_current_time());
			strcpy(reg_node->abs_path_time, abs_path);
			strcat(reg_node->abs_path_time, "_");
			strcat(reg_node->abs_path_time, reg_node->time);
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

			// before add, update backup tree
			if (BACKUP_NODE->child != NULL) {
				reset_backup_tree(BACKUP_NODE->child);
				BACKUP_NODE->child = NULL;
			}
			read_backup(BACKUP_NODE, backup_dir, backup_dir);
			add_child(pNODE, reg_node);
		}
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

void add_child(NODE *parent, NODE *new_node)
{
	NODE *cur = parent;
	if (new_node->is_reg) {
		NODE *backup_node = find_node_by_abs_path(BACKUP_NODE->child, 
				new_node->backup_path);
		if (backup_node != NULL) {
			if (strcmp(hash_func, "md5") == 0) {
				if (strcmp(backup_node->hash_md5, new_node->hash_md5) == 0) {
					printf("\"%s\" is already backuped\n", backup_node->abs_path_time);
					return;
				}
			} else {
				if (strcmp(backup_node->hash_sha1, new_node->hash_sha1) == 0) {
					printf("\"%s\" is already backuped\n", backup_node->abs_path_time);
					return;
				}
			}


			if (backup_node->right == NULL) {
				pid_t pid = fork();
				if (pid == 0) {
					char *args[4] = {0};
					/*
					   args[0] = abs_path
					   args[1] = backup_path
					   args[2] = filename
					   args[3] = time 
					 */
					args[0] = new_node->abs_path;
					args[1] = new_node->backup_path;
					args[2] = new_node->filename;
					args[3] = new_node->time;
					execv("./add", args);
				}
				//while(waitpid(pid, NULL, WNOHANG) == 0) continue;
				waitpid(pid, NULL, 0);

				strcpy(new_node->abs_path, new_node->backup_path);
				strcpy(new_node->abs_path_time, new_node->backup_path);
				strcat(new_node->abs_path_time, "_");
				strcat(new_node->abs_path_time, new_node->time);
				backup_node->right = new_node;
				printf("\"%s_%s\" backuped\n", new_node->abs_path, new_node->time);

				return;
			} else {
				add_sib(backup_node, new_node);
				return;
			}
		} else {

			pid_t pid = fork();
			if (pid == 0) {
				char *args[4] = {0};
				/*
				   args[0] = abs_path
				   args[1] = backup_path
				   args[2] = filename
				   args[3] = time 
				 */
				args[0] = new_node->abs_path;
				args[1] = new_node->backup_path;
				args[2] = new_node->filename;
				args[3] = new_node->time;
				execv("./add", args);
			}
			while(waitpid(pid, NULL, WNOHANG) == 0) continue;
			printf("\"%s_%s\" backuped\n", new_node->abs_path, new_node->time);

		}
	}
	else if (new_node->is_dir) {
		if (strcmp(cur->abs_path, new_node->abs_path) == 0)
			return;

		if (cur->child == NULL) {
			cur->child = new_node;
			mkdir(new_node->backup_path, 0775);
			return;
		} else {
			if (strcmp(cur->child->abs_path, new_node->abs_path) == 0)
				return;
			else {
				add_sib(cur->child, new_node);
				return;
			}
		}
	}

	return;
}

void add_sib(NODE *sib, NODE *new_node)
{
	NODE *cur = sib;

	char *last_token;
	last_token = strrchr(cur->abs_path, '/');
	char cur_filename[4096] = {0};
	strcpy(cur_filename, last_token);

	last_token =  strrchr(new_node->abs_path, '/');
	char new_filename[4096] = {0};
	strcpy(new_filename, last_token);

	if (cur->right == NULL) {
		if (new_node->is_reg) {


			if (strcmp(cur_filename, new_filename) == 0) {
				if (strcmp(hash_func, "md5") == 0) {
					if (strcmp(cur->hash_md5, new_node->hash_md5) == 0) {
						printf("\"%s\" is already backuped\n", cur->abs_path_time);
						return;
					}
				} else {
					if (strcmp(cur->hash_sha1, new_node->hash_sha1) == 0) {
						printf("\"%s\" is already backuped\n", cur->abs_path_time);
						return;
					}
				}
			}

			// cur, new_node are diffent file
			// add file to backup dir
			pid_t pid = fork();
			if (pid == 0) {
				char *args[5] = {0};
				/*
				   args[0] = abs_path
				   args[1] = backup_path 
				   args[2] = filename
				   args[3] = time 
				 */
				args[0] = new_node->abs_path;
				args[1] = new_node->backup_path;
				args[2] = new_node->filename;
				args[3] = new_node->time;
				execv("./add", args);
			}
			while(waitpid(pid, NULL, WNOHANG) == 0) continue;

			// add new_node to backup tree
			strcpy(new_node->abs_path, new_node->backup_path);
			strcpy(new_node->abs_path_time, new_node->backup_path);
			strcat(new_node->abs_path_time, "_");
			strcat(new_node->abs_path_time, new_node->time);
			cur->right = new_node;
			printf("\"%s_%s\" backuped\n", new_node->abs_path, new_node->time);
			return;
		}

		// if cur_dir, new_dir are same dir, return
		// else, mkdir, 
		if (new_node->is_dir && strcmp(cur->abs_path, new_node->abs_path) == 0)
			return;
		else {
			mkdir(new_node->backup_path, 0755);
			cur->right = new_node;
			return;
		}
	}

	// cur->right != NULL
	if (new_node->is_reg) {

		if (strcmp(cur_filename, new_filename) == 0) {
			if (strcmp(hash_func, "md5") == 0) {
				if (strcmp(cur->hash_md5, new_node->hash_md5) == 0) {
					printf("\"%s\" is already backuped\n", cur->abs_path_time);
					return;
				}
			} else {
				if (strcmp(cur->hash_sha1, new_node->hash_sha1) == 0) {
					printf("\"%s\" is already backuped\n", cur->abs_path_time);
					return;
				}
			}
		} else {
			add_sib(cur->right, new_node);
			return;
		}

	}
	// new_node is dir
	add_sib(cur->right, new_node);
	return;

	return;

}
void replace_backup_path(char *dest, char *input) {

	// if input was abs_path
	if (input[0] ==  '/') {
		strcat(dest, backup_dir);
		memcpy(dest + strlen(backup_dir), input + strlen(home_dir), 
				strlen(input) - strlen(home_dir));

	} else {
		// input was rel_path
		char temp[4096] = {0};
		char buf[4096] = {0};
		char *pbuf;
		pbuf = getcwd(buf, 4096);

		memcpy(temp, backup_dir, strlen(backup_dir));
		memcpy(temp + strlen(backup_dir), buf + strlen(home_dir), strlen(buf) - strlen(home_dir));
		strcat(temp, "/");
		strcat(temp, input);

		int count = get_dir_count(temp);
		char **dirs = split_dir(temp, count);

		int top_dir_count = 0;
		int cur_dir_count = 0;

		for(int i=0; i<count; i++) {
			if (strcmp(dirs[i], ".") == 0) cur_dir_count++;
			else if (strcmp(dirs[i], "..") == 0) top_dir_count++;
		}

		for (int i=0; i<top_dir_count; i++) {
			for (int j=0; j<count; j++){
				if (strcmp(dirs[j], "..") == 0) {
					for (int k=j+1; k<count;k++){
						strcpy(dirs[k-2], dirs[k]);
					}
				}
			}
		}
		for(int i=0; i<count - (2 * top_dir_count); i++) { 
			if (strcmp(dirs[i], ".") != 0) {
				strcat(dest, "/");
				strcat(dest, dirs[i]);
			}
		}
		for (int i=0; i<count; i++)
			free(dirs[i]);
		free(dirs);
	}
}

void replace_abs_path(char *dest, char *input) {
	if (input[0] ==  '/') {
		strcpy(dest, input);

	} else {
		char buf[4096] = {0};
		char *pbuf;
		pbuf = getcwd(buf, 4096);

		strcat(dest, buf);
		strcat(dest, "/");
		strcat(dest, input);
	}
}
