#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <arpa/inet.h>     /* inet_ntoa */
#include <netdb.h>  
#include "builtins.h"
#include "io_helpers.h"


// ====== Command execution =====

int sigint_received = 0;
void sigint_handler(int code) {
    sigint_received = 1;
}

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < BUILTINS_COUNT &&
           strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return BUILTINS_FN[cmd_num];
}

int check_bash(char *cmd, char **ret){
    char bin[strlen("/bin/") + strlen(cmd) + 1];
    strcpy(bin, "/bin/");
    strcat(bin, cmd);

    if(access(bin, X_OK) == 0){
        *ret = malloc(sizeof(char) * (strlen(bin) + 1));
        if(*ret == NULL){
            display_error("Error: malloc", "");
            return -1;
        }
        strcpy(*ret, bin);
        return 1;
    }

    char usrbin[strlen("/usr/bin/") + strlen(cmd) + 1];
    strcpy(usrbin, "/usr/bin/");
    strcat(usrbin, cmd);

    if(access(usrbin, X_OK) == 0){
        *ret = malloc(sizeof(char) * (strlen(usrbin) + 1));
        if(*ret == NULL){
            display_error("Error: malloc", "");
            return -1;
        }
        strcpy(*ret, usrbin);
        return 1;
    }

    char usr[strlen("/usr/") + strlen(cmd) + 1];
    strcpy(usr, "/usr/");
    strcat(usr, cmd);

    if(access(usr, X_OK) == 0){
        *ret = malloc(sizeof(char) * (strlen(usr) + 1));
        if(*ret == NULL){
            display_error("Error: malloc", "");
            return -1;
        }
        strcpy(*ret, usr);
        return 1;
    }

    return 0;
}


// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo. 
 */
ssize_t bn_echo(char **tokens, int out, int in) {
    ssize_t index = 1;

    if (tokens[index] != NULL) {
        display_message(tokens[(int)index], out);
        index = index + 1;
    }
    while (tokens[index] != NULL) {
        // TODO:
        // Implement the echo command DONE
        display_message(" ", out);
        display_message(tokens[(int)index], out);
        index += 1;
    }
    display_message("\n", out);

    return 0;
}

ssize_t bn_ls(char **tokens, int out, int in){
    char filter[MAX_INPUT] = "\0";
    int depth = 1;
    int needs_depth = 0;
    char path[MAX_INPUT] = ".";

    ssize_t index = 1;
    while (tokens[index] != NULL){
        if (!strcmp(tokens[index], "--f")){
            if (tokens[index + 1] != NULL){
                strcpy(filter, tokens[index + 1]);
                index = index + 1;
            }else{
                display_error("ERROR: Invalid Filter: ", "NULL");
                return -1;
            }
        }else if (!strcmp(tokens[index], "--rec")){
            needs_depth = needs_depth - 1;
        }else if (!strcmp(tokens[index], "--d")){
            needs_depth = needs_depth + 1;
            if(tokens[index + 1] == NULL){
               display_error("ERROR: Invalid Depth: ", "No Depth Given"); 
               return -1;
            }
            char *end;
            depth = strtol(tokens[index + 1], &end, 10);
            if (depth == 0){
                display_error("ERROR: Invalid Depth: ", "0");
                return -1;
            }
            if(tokens[index + 1] != end - strlen(tokens[index + 1]) || depth == 0){
                display_error("ERROR: Invalid Depth: ", tokens[index + 1]);
                return -1;
            }
            index = index + 1;
        }else {
            if (!strncmp(tokens[index], "~", 1)){
                char *homedir = getenv("HOME");
                strcpy(path, homedir);
                strcat(path, tokens[index] + 1);
                display_message(path, out);
                display_message("\n", out);
            }else {
                stpcpy(path, tokens[index]);
            }
        }
        index = index + 1;
    }

    if (needs_depth == -1){
        display_error("ERROR: Missing arguement: ", "depth");
        return -1;
    }else if (needs_depth == 1){
        display_error("ERROR: Missing arguement: ", "recursion flag");
        return -1;
    }

    if(list_dir(path, depth, filter, 0, out, in) == -1){
        return -1;
    }

    return 0;
}

int list_dir(char *path, int depth, char *filter, int offset, int out, int in){
    DIR *dir = opendir(path);
    struct dirent *entry;

    if (dir == NULL){
        if(errno == ENOENT){
            display_error("ERROR: Invalid path: ", path);
        }
        return -1;
    }
        
    while((entry = readdir(dir)) != NULL){
        if (filter == NULL || strstr(entry -> d_name, filter) != NULL){
            char output[strlen(entry -> d_name) + offset + 1];
            strcpy(output, "\0");
            
            for (int i = 0; i < offset; i++){
                strcat(output, " ");
            }
            strcat(output, entry -> d_name);

            display_message(output, out);
            display_message("\n", out);
        }
        if (depth != 1){
            if (strcmp(entry -> d_name, ".") != 0 && strcmp(entry -> d_name, "..")){
                char rec_path[strlen(path) + 2 + strlen(entry -> d_name)];
                strcpy(rec_path, path);
                strcat(rec_path, "/");
                strcat(rec_path, entry -> d_name);

                DIR *tmp = opendir(rec_path);
                if (tmp != NULL){
                    closedir(tmp);
                    list_dir(rec_path, depth - 1, filter, offset + 1, out, in);
                }
            }
        }

    }

    closedir(dir);
    return 0;
}

ssize_t bn_pwd(char **tokens, int out, int in){
    if (tokens[1] != NULL){
        display_error("ERROR: Too many arguments: ", tokens[1]);
        return -1;
    }
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL){
        display_message(cwd, out);
        display_message("\n", out);
    }else {
        return -1;
    }
    return 0;
}

ssize_t bn_cd(char **tokens, int out, int in){
    if (tokens[1] == NULL){
        display_error("ERROR: Missing Arguments:", "Directory");
        return -1;
    }else if (tokens[2] != NULL){
        display_error("ERROR: Too many Arguments:", tokens[2]);
        return -1;
    } 

    int num_dots = 0;
    while(!strncmp(tokens[1] + num_dots, ".", 1)){
        num_dots = num_dots + 1;
    }
    if (num_dots != 0 && num_dots != 2 && strcmp(tokens[1] + num_dots, "\0")){
        display_error("ERROR: Invalid path: ", tokens[1]);
        return -1;
    }else if(num_dots != 0 && num_dots != 2){
        strcpy(tokens[1], "..");
        for (int i = 1; i < num_dots - 1; i++){
            strcat(tokens[1], "/..");
        }
    }

    if (chdir(tokens[1]) != 0){
        if (errno == ENOENT){
            display_error("ERROR: Invalid path: ", tokens[1]);
        }
        return -1;
    }
    return 0;
}

ssize_t bn_cat(char **tokens, int out, int in){
    FILE *fptr;
    int should_close = 0;
    if(tokens[1] != NULL){
        if(tokens[2] != NULL){
            display_error("ERROR: Too many Arguments:", tokens[2]);
            return -1;
        }
        fptr = fopen(tokens[1], "r");
        if (fptr == NULL){
            display_error("ERROR: ", "Cannot open file");
            return -1;
        }
    }else{
        struct pollfd fds;
        fds.fd = in;     
        fds.events = POLLIN; 
        int ret = poll(&fds, 1, 10); // in your project, set this to 10, not 3000.
        if (ret == 0) {
            display_error("ERROR: ", "No input source provided");
            return -1;
        } else {
            dup2(in, STDIN_FILENO);
            fptr = stdin; 
        }
    }
    char c;
    while ((c = fgetc(fptr)) != EOF){
        char output[2];
        sprintf(output, "%c", c);
        display_message(output, out);
    }

    if(should_close == 1){
       fclose(fptr);
    }

    return 0;
}

ssize_t bn_wc(char **tokens, int out, int in){
    int words = 0, chars = 0, nls = 0;
    int newword = 1;
    char c;
    FILE *fptr;
    int should_close = 0;
    if(tokens[1] != NULL){
        if(tokens[2] != NULL){
            display_error("ERROR: Too many Arguments:", tokens[2]);
            return -1;
        }
        should_close = 1;
        fptr = fopen(tokens[1], "r");
        if (fptr == NULL){
            display_error("ERROR: ", "Cannot open file");
            return -1;
        }
    }else{
        struct pollfd fds;
        fds.fd = in;     
        fds.events = POLLIN; 
        int ret = poll(&fds, 1, 10); // in your project, set this to 10, not 3000.
        if (ret == 0) {
            display_error("ERROR: ", "No input source provided");
            return -1;
        } else {
            dup2(in, STDIN_FILENO);
            fptr = stdin; 
        }
    }
    while ((c = fgetc(fptr)) != EOF){
        chars = chars + 1;
        if (c == '\n'){
            nls = nls + 1;
        }
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r'){
            newword = 1;
        }else if (newword == 1){
            words = words + 1;
            newword = 0;
        }
    }

    if(should_close == 1){
        fclose(fptr);
    }

    char output[2048];
    sprintf(output, "word count %d\ncharacter count %d\nnewline count %d\n", words, chars, nls);
    display_message(output, out);

    

    return 0;

}

ssize_t bn_kill(char **tokens, int out, int in){
    if(tokens[1] == NULL){
        display_error("ERROR: ", "No process specified");
        return -1;
    }
    pid_t pid = strtol(tokens[1], NULL, 10);
    if(kill(pid, 0) == -1 && errno == ESRCH){
        display_error("ERROR: ", "The process does not exist");
        return -1;
    }
    int signal;
    if (tokens[2]== NULL){
        signal = SIGTERM;
    }else{
        signal = strtol(tokens[2], NULL, 10);
    }
    int result = kill(pid, signal);
    if(result == -1 && errno == EINVAL){
        display_error("ERROR: ", "Invalid signal specified");
        return -1;
    }else if(result == -1 && errno == EPERM){
        display_error("ERROR: ", "Permission denied.");
        return -1;
    }
    return 0;
}


struct client_sock {
    int sock_fd;
    char buf[BUF_SIZE];
    int consumed;
    int inbuf;
};

int accept_connection(int fd, struct client_sock **client) {
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;
    
    //fprintf(stderr, "Waiting for a new connection...\n");
    int client_fd = accept(fd, (struct sockaddr *)&peer, &peer_len);
    if (client_fd < 0) {
        perror("accept");
        return -1;
    }
    //fprintf(stderr,
        //"New connection accepted from %s:%d\n",
        //inet_ntoa(peer.sin_addr),
        //ntohs(peer.sin_port));

    *client = malloc(sizeof(struct client_sock));
    (*client)->sock_fd = client_fd;
    (*client)->consumed = (*client)->inbuf = 0;
    memset((*client)->buf, 0, BUF_SIZE);
    
    return client_fd;
}

void clean_exit(struct listen_sock s, struct client_sock *client) {
    if (client) {
        close(client->sock_fd);
        free(client);
    }
    close(s.sock_fd);
    free(s.addr);
}

int setup_server_socket(struct listen_sock *s, int port) {
    if(!(s->addr = malloc(sizeof(struct sockaddr_in)))) {
        display_error("Error: ", "Malloc");
        return -1;
    }
    // Allow sockets across machines.
    s->addr->sin_family = AF_INET;
    // The port the process will listen on.
    s->addr->sin_port = htons(port);
    // Clear this field; sin_zero is used for padding for the struct.
    memset(&(s->addr->sin_zero), 0, 8);
    // Listen on all network interfaces.
    s->addr->sin_addr.s_addr = INADDR_ANY;

    s->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->sock_fd < 0) {
        display_error("Error: ", "Socket");
        return -1;
    }

    // Make sure we can reuse the port immediately after the
    // server terminates. Avoids the "address in use" error
    int on = 1;
    int status = setsockopt(s->sock_fd, SOL_SOCKET, SO_REUSEADDR,
        (const char *) &on, sizeof(on));
    if (status < 0) {
        display_error("Error: ", "setsockopt");
        return -1;
    }

    // Bind the selected port to the socket.
    if (bind(s->sock_fd, (struct sockaddr *)s->addr, sizeof(*(s->addr))) < 0) {
        display_error("Error: ", "Bind");
        close(s->sock_fd);
        return -1;
    }

    // Announce willingness to accept connections on this socket.
    if (listen(s->sock_fd, MAX_BACKLOG) < 0) {
        display_error("Error: ", "Listen");
        close(s->sock_fd);
        return -1;
    }
    return 0;
}

int find_network_newline(const char *buf, int inbuf) {
    for(int i = 0; i < inbuf; i++){
        if(buf[i] == '\r' && buf[i + 1] == '\n'){
            return i + 2;
        }
    }
    return -1;
}

int get_message(char **dst, char *src, int *inbuf) {
    // Implement the find_network_newline() function
    // before implementing this function.
    int newline = find_network_newline(src, *inbuf);
    if (newline == -1){
        return 1;
    }
    *dst = malloc(BUF_SIZE);
    if(*dst == NULL){
        perror("malloc");
        return 1;
    }
    memmove(*dst, src, newline - 2);
    (*dst)[newline - 2] = '\0';
    memmove(src, src + newline, BUF_SIZE - newline);
    *inbuf -= newline;
    return 0;
}

int read_from_socket(int sock_fd, char *buf, int *inbuf) {
    int num_read = read(sock_fd, buf + *inbuf, BUF_SIZE - *inbuf);
    if (num_read == 0){
        return 1;
    }else if (num_read == -1){
        return -1;
    }
    *inbuf += num_read;
    for(int i = 0; i <= *inbuf - 2; i++){
        if(buf[i] == '\r' && buf[i + 1] == '\n'){
            return 0;
        }
    }
    if(*inbuf == BUF_SIZE){
        return -1;
    }
    return 2;
}

ssize_t bn_start_server(char **tokens, int out, int in){
    if(tokens[1] == NULL){
        display_error("ERROR:", "No port provided");
        return -1;
    }
    struct client_sock *clients = NULL;
    
    struct listen_sock s;
    if(setup_server_socket(&s, strtol(tokens[1], NULL, 10)) != 0){
        return -1;
    }

    struct sigaction sa_sigint;
    memset (&sa_sigint, 0, sizeof (sa_sigint));
    sa_sigint.sa_handler = sigint_handler;
    sa_sigint.sa_flags = 0;
    sigemptyset(&sa_sigint.sa_mask);
    sigaction(SIGINT, &sa_sigint, NULL);
    
    int exit_status = 0;

    do {
        int fd = accept_connection(s.sock_fd, &clients);
        if (sigint_received) break;
        if (fd < 0) {
            display_error("Error: ", "Accept");
            continue;
        }

        // Receive messages
        int client_closed;
        do {
            // Step 1: Receive data from client and save into buffer
            // Implement read_from_socket() in socket.c
            client_closed = read_from_socket(clients->sock_fd, clients->buf, &(clients->inbuf));
            if (client_closed == -1) { // Read error
                display_error("Error:", "Recieve");
                clean_exit(s, clients);
                return 1;
            }
            else if (client_closed == 1) { // Client disconnected
                close(fd);
                free(clients);
                clients = NULL;
                //fprintf(stderr, "Client disconnected.\n");
            }
            else if (client_closed == 0) { // Received CRLF
                char *msg;
                // Step 2: Extract each CRLF-terminated message
                // from the buffer into a NULL-terminated string.
                // Free the string once we're done with it.
                // Implement get_message() in socket.c
                display_message("\n", out);
                while (!get_message(&msg, clients->buf, &(clients->inbuf))) {
                    display_message(msg, out);
                    free(msg);
                }
                display_message("\n", out);
                display_message("mysh$", out);
            }
        } while(client_closed != 1); // Loop as long as client is connected
    } while(!sigint_received); // Loop as long as no SIGINT received
    
    clean_exit(s, clients);

    return exit_status;
}

int connect_to_server(int port, const char *hostname) {
    int soc = socket(PF_INET, SOCK_STREAM, 0);
    if (soc < 0) {
        display_error("Error: ", "Socket");
        return -1;
    }
    struct sockaddr_in addr;

    // Allow sockets across machines.
    addr.sin_family = PF_INET;
    // The port the server will be listening on.
    addr.sin_port = htons(port);
    // Clear this field; sin_zero is used for padding for the struct.
    memset(&(addr.sin_zero), 0, 8);

    // Lookup host IP address.
    // gethostbyname is simple but not recommended; 
    // in the assignment we will use getaddrinfo instead
    struct hostent *hp = gethostbyname(hostname);
    if (hp == NULL) {
        display_error("Error: ", "Unknown Host");
        return -1;
    }

    addr.sin_addr = *((struct in_addr *) hp->h_addr);

    // Request connection to server.
    if (connect(soc, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        display_error("Error: ", "Connect");
        return -1;
    }

    return soc;
}

ssize_t bn_send(char **tokens, int out, int in) {
    if (tokens[1] == NULL) {
        display_error("ERROR: Missing Argument: ", "Port");
        return -1;
    }
    if (tokens[2] == NULL) {
        display_error("ERROR: Missing Argument: ", "Hostname");
        return -1;
    }
    if (tokens[3] == NULL) {
        display_error("ERROR: Missing Argument: ", "Message");
        return -1;
    }

    int soc = connect_to_server(strtol(tokens[1], NULL, 10), tokens[2]);

    int i = 3;
    while(tokens[i] != NULL){
        int len = strlen(tokens[i]);
        char buf[len + 3];
        strcpy(buf, tokens[i]);
        buf[len + 2] = '\n';
        buf[len + 1] = '\r';
        buf[len] = ' ';
        write(soc, buf, len + 3);
        i = i + 1;
    }

    close(soc);
    return 0;
}

ssize_t bn_start_client(char **tokens, int out, int in){
    if(tokens[1] == NULL){
        display_error("ERROR: ", "No port provided");
        return -1;
    }
    if(tokens[2] == NULL){
        display_error("ERROR: ", "No hostname provided");
        return -1;
    }

    struct sigaction sa_sigint;
    memset (&sa_sigint, 0, sizeof (sa_sigint));
    sa_sigint.sa_handler = sigint_handler;
    sa_sigint.sa_flags = 0;
    sigemptyset(&sa_sigint.sa_mask);
    sigaction(SIGINT, &sa_sigint, NULL);

    do{
        if(sigint_received){
            break;
        }
        char buf[MAX_STR_LEN + 3];
        if(fgets(buf, MAX_STR_LEN, stdin)==NULL){
            return 0;
        }
        buf[strlen(buf) - 1] = '\0';
        char *ar[5];
        ar[0] = "send";
        ar[1] = tokens[1];
        ar[2] = tokens[2];
        ar[3] = buf;
        ar[4] = NULL;
        bn_send(ar, out, in);
    }while(!sigint_received);
    return 0;
}



int var_set(char *token, struct variable *var, struct varnode *head, int num_vars){
    char *add = strchr(token, '=');
    if(add != NULL && add != token && isdigit(token[0]) == 0){
        int index = (int)(add - token);

        char name[index + 1];
        strncpy(name, token, index);
        name[index] = '\0';

        char value[strlen(token) - index];
        strncpy(value, token + (index * sizeof(char)) + 1, strlen(token) - index);
        value[strlen(token) - index - 1] = '\0';

        int flag = 0;
        struct varnode *on_node = head;
        for (int i = 0; i < num_vars; i++){
            if (strcmp(on_node->value->name, name) == 0){
                strcpy(on_node->value->value, value);
                flag = 1;
                break;
            }
            on_node = on_node->next;
        }
        if (flag == 0){
            strcpy((*var).name, name);
            strcpy((*var).value, value);
        }
        return flag;
    }else{
        display_error("ERROR: Unrecognized command: ", token);
        return -1;
    }
}