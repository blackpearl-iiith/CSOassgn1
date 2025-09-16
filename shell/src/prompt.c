#include "prompt.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <sys/utsname.h>

extern char shell_home_dir[1024];

void display_prompt() {
    struct passwd *pw = getpwuid(getuid());
    const char *username = pw->pw_name;

    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    char display_path[1024];
    if (strstr(cwd, shell_home_dir) == cwd) {
        sprintf(display_path, "~%s", cwd + strlen(shell_home_dir));
    } else {
        strcpy(display_path, cwd);
    }
    printf("<%s@%s:%s>", username, hostname, display_path);
    fflush(stdout);
}