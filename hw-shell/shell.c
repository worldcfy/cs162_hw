#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

typedef struct _arg_cnt {
  int inCnt;
  int inArray[10];
  int outCnt;
  int outArray[10];
  int pipeCnt;
  int pipeArray[10];
} arg_cnt_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print the current working directory"},
    {cmd_cd, "cd", "change working directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* Return the current working directory*/
int cmd_pwd(unused struct tokens* tokens)
{
  char a[100];
  if (getcwd(a, 100) != NULL)
  {
    fprintf(stdout, "%s\n", a);
    return 1;
  }
  else
  {
    return -1;
  }
}

/* Change the current working directory*/
int cmd_cd(struct tokens* tokens)
{
  return chdir(tokens_get_token(tokens, 1));
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Background: 
   * This shell program is executed by the bash shell, so we are doing a shell in a shell
   * approach. In this case, we need to check whether the bash shell is running our shell
   * in the background or foreground because stdin/out/err will only be initially associated
   * with terminal devices when you run a program in the foreground from command line. */

  /* Check if we are running interactively, or have we been provided with some other 
   * input stream(file) */
  shell_terminal = STDIN_FILENO;
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}
/* Pass the location of the redirection, piping or the end of token.
 * Search forward to check for executable and arguments */
void identify_and_execute(int start, int end, char** args)
{
  /* Normal commands goes like: processA arg1 arg2 arg3... */
  char* tok = NULL;
  char* path = getenv("PATH");
  char* saveptr = NULL;
  int lcl_start;
  int lcl_end = end-1;
  int fd;

  if (start == 0)
  {
    lcl_start = start;
  }
  else
  {
    lcl_start = start + 1;
  }

  char* prog = args[lcl_start];

  while (lcl_start != lcl_end)
  {
    if (*args[lcl_end] == '<')
    {
      /* Input redirection */
      fd = open(args[lcl_end+1], O_RDONLY);
      dup2(fd, STDIN_FILENO);
      args[lcl_end] = NULL;
    }
    else if (*args[lcl_end] == '>')
    {
      /* Output redirection */
      fd = open(args[lcl_end+1], O_WRONLY|O_CREAT);
      dup2(fd, STDOUT_FILENO);
      args[lcl_end] = NULL;
    }
    lcl_end--;
  }
  
  if (!access(args[lcl_start], X_OK))
  {
    /* Just execute away!*/
    execv(args[lcl_start], &(args[lcl_start]));
  }
  else
  {
    /* Let's see if the program is in PATH*/
  }
  
  /* Loop through all the paths*/
  tok = strtok_r(path,":",&saveptr);
  while(tok != NULL)
  {
    /* Cannot modify tok, need separate copy*/
    char* full_path = (char*)malloc(strlen(tok)+strlen(prog)+2);
    strcpy(full_path, tok);
    strcat(full_path, "/");
    strcat(full_path, prog);
    if(!access(full_path, X_OK))
    {
      /* Found it!*/
      execv(full_path, &(args[lcl_start]));
    }
    free(full_path);
    tok = strtok_r(NULL,":",&saveptr);
  }

  fprintf(stdout, "Sorry, there is no such prog exist as %s.\n", prog);
}

int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;
  static arg_cnt_t arg_cnt;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Reset the arg_cnt struct */
    arg_cnt.inCnt = 0;
    arg_cnt.outCnt = 0;
    arg_cnt.pipeCnt = 1;
    for(int i = 0; i < 10; i++)
    {
      arg_cnt.inArray[i] = 0;
      arg_cnt.outArray[i] = 0;
      arg_cnt.pipeArray[i] = 0;
    }

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } 
    else {
      /* REPLACE this to run commands as programs. */
      pid_t pid;
      int status;
      char** args = (char**)malloc(sizeof(char*) * (tokens_get_length(tokens)+1));
      int i;
      int pipefd[2];
      int savedIn = dup(STDIN_FILENO);
        
      /* Step 1: break the arguments into individual tokens for processing */
      for(i = 0; i < tokens_get_length(tokens); i++)
      {
        args[i] = tokens_get_token(tokens, i);
        if (*args[i] == '|')
        {
          arg_cnt.pipeArray[arg_cnt.pipeCnt++] = i;
          args[i] = NULL;
        }
        else if (*args[i] == '<')
        {
          arg_cnt.inArray[arg_cnt.inCnt++] = i;
        }
        else if (*args[i] == '>')
        {
          arg_cnt.outArray[arg_cnt.outCnt++] = i;
        }
      }
      args[tokens_get_length(tokens)] = NULL;

      /* Step 2: Create processes for each pipe */
      for(i = 1; i <= arg_cnt.pipeCnt; i++)
      {
        int ret;
        /* Generate a pipe before forking! */
        if (i != arg_cnt.pipeCnt)
          ret = pipe(pipefd);
        pid = fork();

        if ((ret == -1) && (i != arg_cnt.pipeCnt))
        {
          fprintf(stdout, "Piping failed!\n");
        }

        if (pid < 0)
        {
          fprintf(stdout, "Forking failed!\n");
        }
        else if (pid == 0) /* This is the child process*/
        {
          if (i != arg_cnt.pipeCnt)
          {
            close(pipefd[0]); // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO);
            identify_and_execute(arg_cnt.pipeArray[i-1], arg_cnt.pipeArray[i], args);
          }
          else
          {
            /* Last process to execute */
            identify_and_execute(arg_cnt.pipeArray[arg_cnt.pipeCnt-1], tokens_get_length(tokens), args);
          }
        }
        else /* Parent */
        {
          if ( i!= arg_cnt.pipeCnt)
          {
            close(pipefd[1]); // Close the unused write end
            dup2(pipefd[0], STDIN_FILENO);
            //waitpid(pid, &status, 0);
          }
          else
          {
            close(pipefd[0]);
            dup2(savedIn, STDIN_FILENO );
            waitpid(pid, &status, 0);
          }
        }
      }

      /* Step 3: Launch the last process */
      /* Create a new process*/
      //pid = fork();

      //if (pid < 0)
      //{
      //  fprintf(stdout, "Forking failed!\n");
      //}
      //else if (pid == 0) /* This is the child process*/
      //{
      //  identify_and_execute(arg_cnt.pipeArray[arg_cnt.pipeCnt-1], tokens_get_length(tokens), args);
      //}
      //else /* This is the parent process*/
      //{
      //  /* The shell doesn't need to read from pipe */
      //  close(pipefd[0]);
      //  dup2(savedIn, STDIN_FILENO );
      //  waitpid(pid, &status, 0);
      //}
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
