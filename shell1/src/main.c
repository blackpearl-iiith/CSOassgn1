#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <string.h>

#include "parser.h"

void print_prompt(void) {
    char hostname[HOST_NAME_MAX + 1];
    char cwd[PATH_MAX + 1];
    char *username;
    struct passwd *pw;
    char *home_dir;
    size_t home_len;

    pw = getpwuid(getuid());
    username = (pw) ? pw->pw_name : "unknown";

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknownhost", sizeof(hostname));
        hostname[sizeof(hostname) - 1] = '\0';
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strncpy(cwd, "unknownpath", sizeof(cwd));
        cwd[sizeof(cwd) - 1] = '\0';
    }

    home_dir = getenv("HOME");
    if (home_dir == NULL && pw && pw->pw_dir) {
        home_dir = pw->pw_dir;
    }

    if (home_dir != NULL) {
        home_len = strlen(home_dir);
        if (strncmp(cwd, home_dir, home_len) == 0) {
            printf("<%s@%s:~%s> ", username, hostname, cwd + home_len);
            fflush(stdout);
            return;
        }
    }

    printf("<%s@%s:%s> ", username, hostname, cwd);
    fflush(stdout);
}

int main(void) {
    char *line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length;

    while (1) {
        print_prompt();

        line_length = getline(&line, &line_capacity, stdin);
        if (line_length == -1) {
            printf("\nlogout\n");
            break;
        }

        if (line_length > 0 && line[line_length - 1] == '\n') {
            line[line_length - 1] = '\0';
        }

        Token *tokens = NULL;
        int num_tokens = 0;
        if (tokenize(line, &tokens, &num_tokens) == 0) {
            ParserState ps = {tokens, num_tokens, 0};
            if (!parse_shell_cmd(&ps)) {
                printf("Invalid Syntax!\n");
            }
            // else do nothing for valid syntax at this stage
            free_tokens(tokens, num_tokens);
        } else {
            printf("Tokenization error.\n");
        }
    }

    free(line);
    return 0;
}
