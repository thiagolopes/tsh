#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define TSH_RL_BUFSIZE 1024
#define TSH_PS_BUFSIZE 32
#define TSH_TOKENS_PARSER " \n\t\r\a\\"

char *tsh_raw_line() {
  int tsh_rl_bufsize = TSH_RL_BUFSIZE;
  int pos = 0;
  char *buffer = malloc(sizeof(char) * tsh_rl_bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "tsh memory allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    /* read char input */
    c = getchar();

    /* EOF finish the input loop */
    if (c == EOF) {
      buffer[pos] = '\0';
      return buffer;
    }
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
        fprintf(stderr, "tsh memory allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

char **tsh_parse_line(char *raw_line) {
  int tsh_parse_bufsize = TSH_PS_BUFSIZE;
  int pos = 0;
  char *token;
  char **tokens = malloc(sizeof(char *) * tsh_parse_bufsize);

  if (!tokens) {
    fprintf(stderr, "tsh parser memory allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(raw_line, TSH_TOKENS_PARSER);
  while (token != NULL) {
    tokens[pos] = token;
    pos++;

    if (pos == tsh_parse_bufsize) {
      tsh_parse_bufsize += TSH_PS_BUFSIZE;
      tokens = realloc(tokens, tsh_parse_bufsize);

      if (!tokens) {
        fprintf(stderr, "tsh parser memory allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, TSH_TOKENS_PARSER);
  }

  tokens[pos] = NULL;
  return tokens;
}

int tsh_execute(char **args) {
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

void tsh_ps1(void) { printf("> "); }

void tsh_loop(void) {
  char *raw_line;
  char **args;
  int status;

  system("clear");

  do {
    tsh_ps1();

    raw_line = tsh_raw_line();
    args = tsh_parse_line(raw_line);
    status = tsh_execute(args);

    free(raw_line);
    free(args);
  } while (status);
}

int main(int argc, char **argv) {
  tsh_loop();
  return EXIT_SUCCESS;
}
