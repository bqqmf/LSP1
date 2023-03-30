#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <string.h>

typedef struct NODE {
	char abs_path[4096];
	char abs_path_time[4096];
	char backup_path[4096];
	char parent_path[4096];
	char filename[255];
	char hash_md5[100];
	char hash_sha1[100];
	char time[100];

	int is_reg;
	int is_dir;
	off_t size;

	struct NODE *right;
	struct NODE *child;

} NODE;

void add_child(NODE *, NODE *);
void add_sib(NODE *, NODE *);
void add_child_backup(NODE *, NODE *);
void add_sib_backup(NODE *, NODE *);
NODE *find_node(char *);
NODE *find_node_by_abs_path(NODE *, char *);
NODE *find_node_by_backup_path(NODE *, char *);
int get_child_count(NODE *, char *);
NODE **get_childs(NODE *, char *, int);
void print_tree(NODE *);
void reset_backup_tree(NODE *);
