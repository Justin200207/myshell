#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "builtins.h"
#include "io_helpers.h"
#include "mysh.h"

char *prompt = "mysh$ "; // TODO Step 1, Uncomment this. DONE

char input_buf[MAX_STR_LEN + 1];
char *token_arr[MAX_STR_LEN]= {NULL};

int forked;
pid_t cid;

int num_vars;
struct varnode *head;
struct varnode *tail;

int num_bg;
struct bgnode *bghead;
struct bgnode *bgtail;

int stdin_backup;

pid_t server_id = -1;

int ret;

void setup(){
    input_buf[MAX_STR_LEN] = '\0';
    num_vars = 0;
    head = malloc(sizeof(struct varnode));
    tail = head;

    num_bg = 0;
    bghead = malloc(sizeof(struct bgnode));
    bgtail = bghead;
    stdin_backup = dup(STDIN_FILENO);
}

int run(char *input_buf){
        char input_copy[strlen(input_buf) + 1];
        strcpy(input_copy, input_buf);
        char *token = strtok(input_copy, "|");
        char *next_token;
        forked = 0;
        cid = 0;
        int outpipe[2];
        int inpipe[2];

        pipe(inpipe);
        dup2(STDIN_FILENO, inpipe[0]);
            
        while((next_token = strtok(NULL, "|")) != NULL){
            pipe(outpipe);

            forked = 1;

            cid = fork();
                
            if (cid == 0){
                dup2(outpipe[1], STDOUT_FILENO); // Write  outpipe
                close(outpipe[0]); // Do not read from outpipe
                close(inpipe[1]); // Do not write to inpipe
                // Read from inpipe set outside
                break;
            }else{
                wait(&cid);
                cid = 1;
                dup2(outpipe[0], STDIN_FILENO); // Read outpipe
                close(outpipe[1]); // do not write outpipe
                token = next_token;
            }
        }

        size_t token_count = tokenize_input(token, token_arr, head, num_vars);

        // Clean exit
        if (ret != -1 && (token_count == 0 || (strncmp("exit", token_arr[0], 4) == 0))) {
            return 1;
        }

        // Command execution
        char *bash_cmd;
        int check_bash_ret = 0;
        if (token_count >= 1) {
            bn_ptr builtin_fn = check_builtin(token_arr[0]);
            if (builtin_fn != NULL) {
                ssize_t err;
                if (forked == 1 && builtin_fn == bn_cd){
                    err = 0;
                }else{
                    if(builtin_fn == bn_start_server){
                        server_id = fork();
                        if(server_id == 0){
                            err = builtin_fn(token_arr, STDOUT_FILENO, STDIN_FILENO);
                        }else{
                            err = 0;
                        }
                    }else{
                        err = builtin_fn(token_arr, STDOUT_FILENO, STDIN_FILENO);
                    }
                }
                if (err == - 1) {
                    display_error("ERROR: Builtin failed: ", token_arr[0]);
                }
            }else if(strcmp(token_arr[0], "close-server") == 0){ 
                if(server_id != -1){
                    //kill(server_id, SIGINT);
                    server_id = -1;
                }
            }else if(token_count == 1 && strchr(token_arr[0], '=') != NULL){
                struct variable *var = malloc(sizeof(struct variable));        
                int flag = var_set(token_arr[0], var, head, num_vars);
                    
                if (flag == 0){
                    if (num_vars == 0){
                        head->value = var;                
                    } else {
                        tail->next = malloc(sizeof(struct variable));
                        tail->next->value = var;
                        tail = tail->next;
                    }
                    num_vars += 1;
                }else {
                    free(var);
                }
            } else if (strcmp(token_arr[0], "ps") == 0){
                bgnode *on_node = bghead;
                for(int i = 0; i < num_bg; i++){
                    char buf[256];
                    sprintf(buf, "%s %d\n", on_node->name, on_node->id);
                    display_message(buf, STDOUT_FILENO);
                    on_node = on_node->next;
                }
            }else if ((check_bash_ret = check_bash(token_arr[0], &bash_cmd)) != 0){
                pid_t pid = fork();
                if(pid == 0){
                    execv(bash_cmd, token_arr);
                }else{
                    wait(&pid);
                }

                free(bash_cmd);
            }else if(check_bash_ret == -1){
                display_error("ERROR: malloc", "");
            }else{
                display_error("ERROR: Unrecognized command: ", token_arr[0]);
            }
        }
        return 0;
}

void handle_c(int code){
    display_message("\n", STDOUT_FILENO);
}

int main(int argc, char* argv[]) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handle_c;
    sigaddset(&sa.sa_mask, SIGINT);
    sigaction(SIGINT, &sa, NULL);

    setup();
    
    int new_id = -2;
    int bg = 0;
    char cmdbackup[128];

    while (new_id != 0) {
        cmdbackup[0] = '\0';
        //my_num = -1;
        new_id = -2;
        // Prompt and input tokenization

        // TODO Step 2: DONE
        // Display the prompt via the display_message function.
        display_message(prompt, STDOUT_FILENO);

        dup2(STDIN_FILENO, stdin_backup);

        ret = get_input(input_buf);

        int index_amp = check_bg(input_buf);
        if (index_amp != -1){
            input_buf[index_amp] = '\0';
            new_id = fork();
            if(new_id == -1){
                perror("fork");
            }
            if(new_id != 0){
                if (num_bg == 0){
                    bghead->id = new_id;
                    bghead->done = 0;
                    strcpy(bghead->name,input_buf);             
                } else {
                    bgtail->next = malloc(sizeof(struct bgnode));
                    bgtail->next->id = new_id;
                    strcpy(bgtail->next->name, input_buf);
                    bgtail->next->done = 0;
                    bgtail = bgtail->next;
                }
                num_bg += 1;
            }else{
                bg = 1;
                strcpy(cmdbackup, input_buf);
            }
        }

        int exit = 0;
        
        if(new_id == -2 || new_id == 0){
            exit = run(input_buf);
        }else{
            char buf[32];
            sprintf(buf, "[%d] %d\n", num_bg, new_id);
            display_message(buf, STDOUT_FILENO);
        }
                    
        if (exit == 1 || (cid == 0 && forked == 1) || new_id == 0){
            break;
        }

        if(bg == 0){
            struct bgnode *on_node = bghead;
            int not_done = 0;
            for (int i = 0; i < num_bg; i++){
                if(on_node->done == 0){
                    int status;
                    pid_t wpid = waitpid(on_node->id, &status, WNOHANG);
                    if (on_node->done == 0 && wpid == on_node->id){
                        char buf[256];
                        sprintf(buf, "[%d]+  Done %s\n", i + 1, on_node->name);
                        display_message(buf, STDOUT_FILENO);
                        on_node->done = 1;
                    }else if(on_node->done == 0){
                        not_done = 1;
                    }
                    on_node = on_node->next;
                }
            }
            if (not_done == 0){
                if (num_bg == 0){
                    free(bghead);
                } else {
                    struct bgnode *on_node = bghead;
                    for (int i = 0; i < num_bg; i++){
                        struct bgnode *temp_node = on_node -> next;
                        free(on_node);
                        on_node = temp_node;
                    }
                }
                bghead = malloc(sizeof(struct bgnode));
                bgtail = bghead;
                num_bg = 0;
            }
        }

        dup2(stdin_backup, STDIN_FILENO);
    }   

    if (num_vars == 0){
        free(head);
    } else {
        struct varnode *on_node = head;
        for (int i = 0; i < num_vars; i++){
            struct varnode *temp_node = on_node->next;
            free(on_node->value);
            free(on_node);
            on_node = temp_node;
        }
    }
    if (num_bg == 0){
        free(bghead);
    } else {
        struct bgnode *on_node = bghead;
        for (int i = 0; i < num_bg; i++){
            struct bgnode *temp_node = on_node -> next;
            free(on_node);
            on_node = temp_node;
        }
    }
    if(server_id != -1){
        //kill(server_id, SIGINT);
    }
    return 0;
}
