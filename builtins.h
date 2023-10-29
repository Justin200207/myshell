#ifndef __BUILTINS_H__
#define __BUILTINS_H__

#define MAX_BACKLOG 5
#define BUF_SIZE 30

#include <unistd.h>

struct listen_sock {
    struct sockaddr_in *addr;
    int sock_fd;
};


typedef struct variable
{
  char name[64];
  char value[64];  
} variable;

typedef struct varnode {
    struct variable *value;
    struct varnode *next;
} varnode;

/* Type for builtin handling functions
 * Input: Array of tokens
 * Return: >=0 on success and -1 on error
 */
typedef ssize_t (*bn_ptr)(char **, int, int);
ssize_t bn_echo(char **tokens, int out, int in);
ssize_t bn_ls(char **tokens, int out, int in);
ssize_t bn_pwd(char **tokens, int out, int in);
int list_dir(char *path, int depth, char *filter, int offset, int out, int in);
ssize_t bn_cd(char **tokens, int out, int in);
ssize_t bn_cat(char **tokens, int out, int in);
ssize_t bn_wc(char **tokens, int out, int in);
ssize_t bn_kill(char **tokens, int out, int in);
ssize_t bn_start_server(char **tokens, int out, int in);
ssize_t bn_send(char **tokens, int out, int in);
ssize_t bn_start_client(char **tokens, int out, int in);


/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd);
int check_bash(char *cmd, char **ret);


/* BUILTINS and BUILTINS_FN are parallel arrays of length BUILTINS_COUNT
 */
static const char * const BUILTINS[] = {"echo", "ls", "pwd", "cd", "cat", "wc", "kill", "start-server", "send", "start-client"};
static const bn_ptr BUILTINS_FN[] = {bn_echo, bn_ls, bn_pwd, bn_cd, bn_cat, bn_wc, bn_kill, bn_start_server, bn_send, bn_start_client, NULL};    // Extra null element for 'non-builtin'
static const size_t BUILTINS_COUNT = sizeof(BUILTINS) / sizeof(char *);
int var_set(char *token, struct variable *var, struct varnode *head, int num_var);

#endif
