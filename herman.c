#include "herman_log.h"
#include "my_lib.h"

#define MAX_ARGS 1024
#define MAX_HISTORY 500
#define KEEP_LAST 10
#define MAX_JOBS 100

volatile sig_atomic_t child_pid = 0; //pid1
volatile sig_atomic_t child_pid_grep = 0; //pid_grep

char file_path[1024]; // it must be global variable. because I use it in
                      // read_command().

int is_bg; // for bg/fg proccess

char global_prompt[2048] = "";

void update_prompt() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        log_error("getcwd failed");
        strcpy(global_prompt, "\033[1;32mherman\033[0m$ "); 
        return;
    }

    const char *home = getenv("HOME");
    char display_path[1024];

    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(display_path, sizeof(display_path), "~%s", cwd + strlen(home));
    } else {
        snprintf(display_path, sizeof(display_path), "%s", cwd);
    }

    snprintf(global_prompt, sizeof(global_prompt),
             "\033[1;32mherman\033[0m:\033[1;34m%s\033[0m$ ",
             display_path);
}

void check_trim_history(const char *history_path) {
  FILE *fp = fopen(history_path, "r");
  if (!fp)
    return;

  char **lines = malloc(sizeof(char *) * MAX_HISTORY * 2);
  int count = 0;
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), fp)) {
    lines[count] = strdup(buffer);
    count++;
  }
  fclose(fp);

  if (count <= MAX_HISTORY) {
    for (int i = 0; i < count; i++)
      free(lines[i]);
    free(lines);
    return;
  }

  fp = fopen(history_path, "w");
  if (!fp) {
    for (int i = 0; i < count; i++)
      free(lines[i]);
    free(lines);
    return;
  }

  int start = count - KEEP_LAST;
  if (start < 0)
    start = 0;

  for (int i = start; i < count; i++) {
    fprintf(fp, "%s", lines[i]);
  }

  fclose(fp);

  for (int i = 0; i < count; i++) {
    free(lines[i]);
  }
  free(lines);
}

void print_random_joke(void) {
  FILE *fp = popen("curl -s -H \"Accept: application/json\" "
                   "https://icanhazdadjoke.com/ | jq -r '.joke'",
                   "r");
  if (!fp) {
    perror("popen");
    return;
  }

  char joke[512];
  if (fgets(joke, sizeof(joke), fp)) {
    printf("\n@@ Joke of the Day:\n%s\n\n", joke);
  } else {
    printf(
        "\n@@ Joke of the Day:\n(No internet - could not fetch a joke.)\n\n");
  }
  pclose(fp);
}

int is_empty_or_whitespace(const char *s) {
  while (*s) {
    if (*s != ' ' && *s != '\t' && *s != '\n')
      return 0;
    s++;
  }
  return 1;
}

char *read_command() {
  // printf("herman>> ");
  fflush(stdout);
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));

  const char *home = getenv("HOME");
  char display_path[1024];

  if (home && strncmp(cwd, home, strlen(home)) == 0) {
    snprintf(display_path, sizeof(display_path), "~%s", cwd + strlen(home));
  } else {
    snprintf(display_path, sizeof(display_path), "%s", cwd);
  }

  update_prompt();
  char rl_prompt[4096];
  snprintf(rl_prompt, sizeof(rl_prompt),
             "\001\033[1;32m\002herman\001\033[0m\002:\001\033[1;34m\002%s\001\033[0m\002$ ",
             global_prompt + strlen("\033[1;32mherman\033[0m:"));
  char *line = NULL;
  line = readline(rl_prompt);
  // size_t len = 0;
  // getline(&line, &len, stdin);

  if (line) {
    line[strcspn(line, "\n")] = '\0';
  }

  if (!line) {
    printf("\n");
    exit(0);
  }

  add_history(line);

  if (!is_empty_or_whitespace(line)) {
    FILE *fp = fopen(file_path, "a");
    if (fp) {
      fprintf(fp, "%s\n", line);
      fclose(fp);
    }
    check_trim_history(file_path); // Check limitation for history file
  }

  return line;
}

char **parse_command(char *line) { // manage command on arry for use in execvp
  char **args = malloc(sizeof(char *) * MAX_ARGS);
  int i = 0;
  char *token = strtok(line, " \t\n");
  while (token != NULL) {
    args[i++] = token;
    token = strtok(NULL, " \t\n");
  }
  args[i] = NULL;
  return args;
}

void handle_sigint(int sig) {
  log_msg("Received SIGINT, child_pid=%d, child_pid_grep=%d", child_pid, child_pid_grep);
  if (child_pid > 0) {
      kill(child_pid, SIGKILL);
  }
    if (child_pid_grep > 0) {
        kill(child_pid_grep, SIGKILL);
    }
    write(STDOUT_FILENO, "\n", 1);
    write(STDOUT_FILENO, global_prompt, strlen(global_prompt));
    rl_forced_update_display();
}

void handle_sigchld(int sig) {
  log_msg("Received SIGCHLD");
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        log_msg("Reaped a child process");
    }
}

void handle_sigtstp(int sig) {
    log_msg("Received SIGTSTP, child_pid=%d, child_pid_grep=%d", child_pid, child_pid_grep);
    if (child_pid > 0) {
        kill(child_pid, SIGTSTP);
        write(STDOUT_FILENO, "\n[1]+ stopped (pid=", 19);
        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "%d)\n", child_pid);
        write(STDOUT_FILENO, pid_str, strlen(pid_str));
        write(STDOUT_FILENO, global_prompt, strlen(global_prompt));
        rl_forced_update_display();
    }
    if (child_pid_grep > 0) {
        kill(child_pid_grep, SIGTSTP);
        write(STDOUT_FILENO, "\n[2]+ stopped (pid=", 19);
        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "%d)\n", child_pid_grep);
        write(STDOUT_FILENO, pid_str, strlen(pid_str));
        write(STDOUT_FILENO, global_prompt, strlen(global_prompt));
        rl_forced_update_display();
    }
}

typedef struct {
  pid_t pid;
  char command[256];
  int is_running;
} job;

static job jobs[MAX_JOBS];
static int job_count = 0;

static int add_job(pid_t pid, const char *cmd, int running) {
  if (job_count >= MAX_JOBS)
    return -1;
  jobs[job_count].pid = pid;
  strncpy(jobs[job_count].command, cmd ? cmd : "",
          sizeof(jobs[job_count].command) - 1);
  jobs[job_count].command[sizeof(jobs[job_count].command) - 1] = '\0';
  jobs[job_count].is_running = running;
  return job_count++;
}

static int find_last_stopped_job(void) {
  for (int i = job_count - 1; i >= 0; --i) {
    if (jobs[i].pid > 0 && jobs[i].is_running == 0)
      return i;
  }
  return -1;
}

static int find_job_by_pid(pid_t pid) {
  for (int i = 0; i < job_count; i++)
    if (jobs[i].pid == pid)
      return i;
  return -1;
}

static void remove_job_by_pid(pid_t pid) {
  int i = find_job_by_pid(pid);
  if (i < 0)
    return;
  for (int k = i + 1; k < job_count; k++)
    jobs[k - 1] = jobs[k];
  job_count--;
}

int main() {
  const char *home = getenv("HOME");
  if (home == NULL) {
    fprintf(stderr, "HOME not set\n");
    exit(1);
  }

  char dir_path[1024];
  snprintf(dir_path, sizeof(dir_path), "%s/.herman", home);

  // char file_path[1024]; local variable not work here.
  snprintf(file_path, sizeof(file_path), "%s/history", dir_path);

  struct stat st = {0};
  if (stat(dir_path, &st) == -1) {
    mkdir(dir_path, 0700);
  }

  using_history();
  read_history(file_path);

  FILE *fp = fopen(file_path, "a");
  if (!fp) {
    perror("Cannot creat history file");
    exit(1);
  }
  fclose(fp);

  struct sigaction sa; // sa -> for sigint
  sa.sa_handler = &handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  struct sigaction sa2; // sa2 -> for sigchld
  sa2.sa_handler = &handle_sigchld;
  sigemptyset(&sa2.sa_mask);
  sa2.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa2, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  struct sigaction sa3; // sa3 -> for sigtstp
  sa3.sa_handler = &handle_sigtstp;
  sigemptyset(&sa3.sa_mask);
  sa3.sa_flags = 0;
  if (sigaction(SIGTSTP, &sa3, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  print_random_joke();

  while (1) {
    char *line = read_command();

    char **my_command = parse_command(line);

    if (my_command[0] == NULL) {
      free(my_command);
      free(line);
      continue;
    }

    if (strcmp(my_command[0], "pwd") ==
        0) { // for pwd. actually this diff between addres of my_command and pwd
      char cwd[1024];
      if (getcwd(cwd, sizeof(cwd)) != NULL) { // this func give us current path
        printf("%s\n", cwd);
      } else {
        perror("getcwd error\n");
      }
      continue;
    }

    if (strcmp(my_command[0], "cd") == 0) {
      if (my_command[1] == NULL) {
        char *home = getenv("HOME");
        if (home != NULL) {
          if (chdir(home) != 0) {
            perror("cd");
          }
        } else {
          fprintf(stderr, "HOME not set\n");
        }
      } else {
        if (chdir(my_command[1]) != 0) {
          perror("cd");
        }
      }
      continue;
    }

    if (strcmp(my_command[0], "history") == 0) {
      FILE *fp = fopen(file_path, "r");
      if (fp) {
        char buffer[1024];
        int count = 1;
        while (fgets(buffer, sizeof(buffer), fp)) {
          printf("%d %s", count++, buffer);
        }
        fclose(fp);
      } else {
        perror("history");
      }
      continue;
    }

    if (strcmp(my_command[0], "exit") == 0) {
      break;
    }

    if (strcmp(my_command[0], "echo") == 0) {
      if (my_command[1] == NULL) {
        printf("\n");
      } else {
        for (int i = 1; my_command[i] != NULL; i++) {
          printf("%s", my_command[i]);
          if (my_command[i + 1] != NULL) {
            printf(" ");
          }
        }
        printf("\n");
      }
      continue;
    }

    if (strcmp(my_command[0], "data") == 0) {
      time_t t = time(NULL);
      printf("%s", ctime(&t));
      continue;
    }

    if (strcmp(line, "clear") == 0) {
      printf("\033[H\033[J");
      continue;
    }
    
    int pipe_index = -1; //for pipe line
    int redirect_index = -1; // redirection for >
    for (int i = 0; my_command[i] != NULL; i++) {
      if (strcmp(my_command[i], ">") == 0) {
        redirect_index = i;
      } else if (strcmp(my_command[i], "|") == 0) {
      	pipe_index = i;
      }
    }

    char **cmd1 = NULL;
    char **cmd2 = NULL;
    int fd_pipeline[2] = {-1, -1};
    int fd[2] = {-1, -1};

    if (pipe_index != -1) {
      my_command[pipe_index] = NULL;
      cmd1 = my_command;
      cmd2 = &my_command[pipe_index + 1];

      if (cmd1[0] == NULL || cmd2[0] == NULL) {
        log_error("syntax error: invalid pipeline");
        free(my_command);
        free(line);
        continue;
      }

      if (pipe(fd_pipeline) < 0) {
        log_error("pipe creation failed");
        free(my_command);
        free(line);
        continue;
      }
    }

    if (redirect_index != -1 && pipe_index != -1) {
	log_error("Redirection with pipe not supported yet");
    	free(my_command);
    	free(line);
    	continue;
    }
    
    if (redirect_index == -1 && pipe_index == -1) {
    	if (pipe(fd) < 0) {
            log_error("pipe creation failed for capturing output");
            free(my_command);
            free(line);
            continue;
        }  
    }

    pid_t pid1 = fork();
    if (pid1 < 0) {
      log_error("fork failed for cmd1");
      if (pipe_index != -1) {
        close(fd_pipeline[0]);
        close(fd_pipeline[1]);
      }
      if (redirect_index == -1 && pipe_index == -1) {
      	close(fd[0]);
	close(fd[1]);
      }
      free(my_command);
      free(line);
      continue;
    }

    if (pid1 == 0) { // child cmd1
    if (pipe_index != -1) {
        close(fd_pipeline[0]);
        if (dup2(fd_pipeline[1], STDOUT_FILENO) < 0) {
            log_error("dup2 failed for child pid=%d", getpid());
            _exit(1);
        }
        close(fd_pipeline[1]);
        execvp(cmd1[0], cmd1);
        log_error("execvp failed for cmd1=%s", cmd1[0]);
        _exit(127);
    }

    if (redirect_index != -1) {
        if (my_command[redirect_index + 1] == NULL) {
            log_error("No file specified for redirection");
            _exit(1);
        }
        int fd_redirect = open(my_command[redirect_index + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_redirect < 0) {
            log_error("open failed for redirection file: %s",
                      my_command[redirect_index + 1]);
            _exit(1);
        }
        my_command[redirect_index] = NULL;
        if (dup2(fd_redirect, STDOUT_FILENO) < 0) {
            log_error("dup2 failed for redirection");
            close(fd_redirect);
            _exit(1);
        }
        close(fd_redirect);
        execvp(my_command[0], my_command);  
        log_error("execvp failed for cmd1=%s", my_command[0]);
        _exit(127);
    } else if (pipe_index == -1) {  
    	    close(fd[0]);
            if (dup2(fd[1], STDOUT_FILENO) < 0) {
            log_error("dup2 failed for output pipe");
            _exit(1);
        }
        close(fd[1]);
        if (strcmp(my_command[0], "ls") == 0 && my_command[1] == NULL) {
            char *args[] = {"ls", "--color=always", "-C", NULL};
            execvp(args[0], args);
            log_error("execvp failed for ls");
            _exit(127);
        } else {
            execvp(my_command[0], my_command);
            log_error("execvp failed for cmd1=%s", my_command[0]);
            _exit(127);
        }
    }
  }

    // parent process
    child_pid = pid1;

    log_msg("Parent pid=%d waiting for child pid1=%d", getpid(), pid1);

    pid_t pid_grep = -1;
    if (pipe_index != -1) {
      log_msg("About to fork for cmd2: %s", cmd2[0]);
      pid_grep = fork();
      if (pid_grep < 0) {
        log_error("fork failed for cmd2");
        kill(pid1, SIGKILL);
        close(fd_pipeline[0]);
        close(fd_pipeline[1]);
        free(my_command);
        free(line);
        continue;
      }

      if (pid_grep == 0) { // child cmd2
        log_msg("Child pid=%d executing cmd2: %s", getpid(), cmd2[0]);
        if (dup2(fd_pipeline[0], STDIN_FILENO) < 0) {
          log_error("dup2 failed for child pid=%d cmd2", getpid());
          _exit(1);
        }
        close(fd_pipeline[0]);
        close(fd_pipeline[1]);
        execvp(cmd2[0], cmd2);
        log_error("execvp failed for cmd2=%s", cmd2[0]);
        _exit(127);
      }
      child_pid_grep = pid_grep;
    }

    // parent closes pipeline fds
    if (pipe_index != -1) {
      close(fd_pipeline[0]);
      close(fd_pipeline[1]);
    }

    // read fd if needed
    if (redirect_index == -1 && pipe_index == -1) {
    	close(fd[1]);
    	char buffer[4096];
    	ssize_t n;
   	while ((n = read(fd[0], buffer, sizeof(buffer) - 1)) > 0) {
        	buffer[n] = '\0';
        	printf("%s", buffer);
    	}
    	close(fd[0]);
    }

    // wait for children
    int status1 = 0, status2 = 0;
    waitpid(pid1, &status1, 0);
    if (pid_grep != -1)
      waitpid(pid_grep, &status2, 0);

    log_msg("Parent finished waiting: pid1=%d status=%d, pid_grep=%d status=%d",
            pid1, status1, pid_grep, status2);

    if (WIFEXITED(status1)) {
      int code = WEXITSTATUS(status1);
      log_msg("Child pid1=%d exited with code %d", pid1, code);
    }
    if (pipe_index != -1 && WIFEXITED(status2)) {
      int code2 = WEXITSTATUS(status2);
      log_msg("Child pid_grep=%d exited with code %d", pid_grep, code2);
    }

    child_pid = 0;
    child_pid_grep = 0;

    free(my_command);
    free(line);
    continue;
  }
  return 0;
}
