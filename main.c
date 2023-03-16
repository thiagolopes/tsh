#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define TSH_RL_BUFSIZE 1024
#define TSH_PS_BUFSIZE 8
#define TSH_PIPE_BUFSIZE 5
#define TSH_TOKENS_PARSER " \n\t\r\a\\"
#define TSH_TOKENS "|"

#define RESET "\x1B[0m"
#define BOLD "\x1B[1m"

/* TODO implement clear command-l */
/* TODO implement up arrow navegate to a memory history */

int tsh_builtin_exit(char **args);
int tsh_builtin_cd(char **args);
int tsh_builtin_help(char **args);

typedef struct {
    char *builtin_name;
    int (*builtin_functions)(char **);
} builtin;

builtin BUILTINS[] = {
    {"exit", &tsh_builtin_exit},
    {"cd", &tsh_builtin_cd},
    {"help", &tsh_builtin_help},
};

int BUILTIN_LEN = sizeof(BUILTINS) / sizeof(builtin);

int tsh_builtin_exit(char **args) {
    exit(EXIT_SUCCESS);
}

int tsh_builtin_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "tsh: cd expect one argument\n");
    }

    if (chdir(args[1]) != 0) {
        perror("tsh: ");
    }

    return 1;
}

int tsh_builtin_help(char **args) {
    printf("Thiago's shell, TSH\n\n");
    printf("Type arguments and hit enter.\n");
    printf("Use \\ to break line.\n\n");
    printf("Commands builtins:\n");

    for (int i = 0; i < BUILTIN_LEN; i++) {
        printf("\t %s\n", BUILTINS[i].builtin_name);
    }

    printf("\nUse man to more information\n\n");
    return 1;
}

char *tsh_raw_line() {
    int tsh_rl_bufsize = TSH_RL_BUFSIZE;
    int pos = 0;
    char *buffer = malloc(sizeof(char) * tsh_rl_bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "tsh: memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        /* read char input */
        c = getchar();

        /* TODO check if is keybind <control> */

        /* EOF finish the input loop */
        if (c == EOF) {
            buffer[pos] = '\0';
            return buffer;
        }
        /* unless is not a break line, \n finish the input loop  */
        if (c == '\n' && (!(pos > 0 && buffer[pos - 1] == '\\'))) {
            buffer[pos] = '\0';
            return buffer;
        }

        buffer[pos] = c;
        pos++;

        if (pos == tsh_rl_bufsize) {
            tsh_rl_bufsize *= 2;
            buffer = realloc(buffer, tsh_rl_bufsize);
            if (!buffer) {
                fprintf(stderr, "tsh: memory allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

char ***tsh_parse_line(char **raw_lines, int lines_len) {
    char ***args = malloc(lines_len * sizeof(char**));

    if (!args) {
        fprintf(stderr, "tsh: memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    for (int i=0; i < lines_len; i++) {
        int pos = 0;
        int tsh_rl_bufsize = TSH_PS_BUFSIZE;
        char *token;

        args[i] = malloc(tsh_rl_bufsize * sizeof(int));
        token = strtok(raw_lines[i], TSH_TOKENS_PARSER);

        while(token != NULL) {
            args[i][pos] = token;
            pos ++;

            if (pos == tsh_rl_bufsize) {
                tsh_rl_bufsize *= 2;
                args[i] = realloc(args[i], tsh_rl_bufsize);
                if (!args[i]) {
                    fprintf(stderr, "tsh: memory allocation error\n");
                    exit(EXIT_FAILURE);
                }
            }

            token = strtok(NULL, TSH_TOKENS_PARSER);
        }
    }
    return args;
}


char **tsh_parse_tokens(char *raw_line, int *lines_len) {
    int tsh_parse_bufsize = TSH_PIPE_BUFSIZE;
    int pos = 0;
    char *line;
    char **pipes = malloc(tsh_parse_bufsize * sizeof(char *));

    if (!pipes) {
        fprintf(stderr, "tsh: parser memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    line = strtok(raw_line, "|");

    while(line) {
        pipes[pos] = line;
        pos++;

        if (pos == tsh_parse_bufsize) {
            tsh_parse_bufsize += TSH_PS_BUFSIZE;
            pipes = realloc(pipes, tsh_parse_bufsize);
            if (!pipes) {
                exit(EXIT_FAILURE);
            }
        }
        line = strtok(NULL, "|");
    }
    *lines_len = pos;
    return pipes;
}


int tsh_spawn(char **args, int std_in, int std_out) {
    /* builtin command */
    for (int i = 0; i < BUILTIN_LEN; i++) {
        if (strcmp(BUILTINS[i].builtin_name, args[0]) == 0) {
            return (*BUILTINS[i].builtin_functions)(args);
        }
    }

    /* normal command */
    pid_t pid, wpid;
    int status;

    pid = fork();

    if (pid == 0) {
        // child
        if (execvp(args[0], args) == -1) {
            perror("tsh: ");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // fork error
        perror("tsh: ");
    } else {
        // parent
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
            if (wpid == -1) {
                perror("tsh: ");
                exit(EXIT_FAILURE);
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

int tsh_execute(char ***args, int lines_len) {
    int status = 1, fd[2], std_in = STDIN_FILENO, std_out = STDOUT_FILENO;
    pipe(fd);

    for (int i=0; i < lines_len; i++) {
        status = tsh_spawn(args[i], std_in, std_out);
        if (!status) {
            perror("tsh: ");
            exit(EXIT_FAILURE);
        }
    }
    close(fd[0]);
    close(fd[1]);
    return status;
}

void tsh_ps1(void) {
    char *pwd;
    pwd = getcwd(pwd, PATH_MAX);
    printf("%s%s%s> ", BOLD, pwd, RESET=);
}

void tsh_loop(void) {
    char *raw_line, **raw_lines, ***args;
    int status;

    system("clear");

    do {
        int lines_len = 0;

        tsh_ps1();
        raw_line = tsh_raw_line();
        raw_lines = tsh_parse_tokens(raw_line, &lines_len);
        args = tsh_parse_line(raw_lines, lines_len);
        status = tsh_execute(args, lines_len);

        for (int i=0; i < lines_len; i++) {
            free(args[i]);
        }
        free(raw_line);
        free(raw_lines);
        free(args);

    } while (status);
}

int main(int argc, char *argv[]) {
    tsh_loop();
    return EXIT_SUCCESS;
}
