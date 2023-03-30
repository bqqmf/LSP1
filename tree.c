#include "tree.h"




void add_child_backup(NODE *parent, NODE *new_node)
{
	NODE *cur = parent;

	if (parent->child == NULL) {
		parent->child = new_node;
		return;
	} 
	if (strcmp(cur->child->abs_path, new_node->abs_path) == 0 &&
			strcmp(cur->child->hash_md5, new_node->hash_md5) == 0 &&
			strcmp(cur->child->hash_sha1, new_node->hash_sha1) == 0) {
		return;
	} else
		add_sib_backup(cur->child, new_node);
	return;
}

void add_sib_backup(NODE *sib, NODE *new_node)
{
	NODE *cur = sib;

	if (cur->right == NULL) {
		cur->right = new_node;
		return;
	}

	if (strcmp(cur->right->abs_path, new_node->abs_path) == 0 && 
			strcmp(cur->right->hash_md5, new_node->hash_md5) == 0 && 
			strcmp(cur->right->hash_sha1, new_node->hash_sha1) == 0) {
	} else
		add_sib_backup(cur->right, new_node);
	return;

}

NODE *find_node_by_abs_path(NODE *start, char *parent_path)
{
	NODE *cur = start;

	if (cur == NULL) return NULL;

	// cur->abs_path find by abs_path.
	//if (strcmp(cur->abs_path, parent_path) == 0) return cur;
	if (strcmp(cur->abs_path, parent_path) == 0) return cur;

	NODE *child = find_node_by_abs_path(cur->child, parent_path);
	NODE *right = NULL;

	if (child == NULL) {
		right = find_node_by_abs_path(cur->right, parent_path);
	}

	if (child != NULL) return child;
	if (right != NULL) return right;

}
// return NODE by abs_path
NODE *find_node_by_backup_path(NODE *start, char *parent_path)
{
	NODE *cur = start;

	if (cur == NULL) return NULL;

	if (strcmp(cur->backup_path, parent_path) == 0) return cur;

	NODE *child = find_node_by_backup_path(cur->child, parent_path);
	NODE *right = NULL;

	if (child == NULL) {
		right = find_node_by_backup_path(cur->right, parent_path);
	}

	if (child != NULL) return child;
	if (right != NULL) return right;

}

int get_child_count(NODE *parent, char *child_name) {
	int count = 0;
	if (parent->child == NULL) return 0;

	NODE *cur = parent->child;

	while (cur != NULL) {
		if (strcmp(cur->abs_path, child_name) == 0) count ++;
		cur = cur->right;
	}
	return count;
}

NODE **get_childs(NODE *parent, char *child_name, int count) {
	NODE **temp = (NODE **)calloc(count, sizeof(NODE *));
	for (int i=0; i<count; i++)
		temp[i] = (NODE *)calloc(1, sizeof(NODE));

	if (count == 0) return NULL;

	NODE *cur = parent->child;
	int idx = 0;

	while (cur != NULL) {
		if (strcmp(cur->abs_path, child_name) == 0) {
			temp[idx++] = cur;
		}
		cur = cur->right;
	}

	return temp;
}



void print_tree(NODE *start) 
{
	NODE *cur = start;

	if (cur == NULL) return;

	if (cur->is_dir)
		printf("%s\n", cur->abs_path);
	else
		printf("%s\n", cur->abs_path_time);
	print_tree(cur->child);
	print_tree(cur->right);

}

void reset_backup_tree(NODE *start) {
	NODE *cur = start;

	if (cur == NULL) return;

	reset_backup_tree(cur->child);
	reset_backup_tree(cur->right);
	free(cur);

}
