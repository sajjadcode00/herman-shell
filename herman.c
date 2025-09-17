#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h> 


#define MAX_ARGS 1024
#define MAX_HISTORY 500
#define KEEP_LAST 10
#define MAX_JOBS 100

volatile sig_atomic_t child_pid = 0;

char file_path[1024]; // it must be global variable. because I use it in read_command().
		
int is_bg; // for bg/fg proccess

void check_trim_history(const char* history_path) {
	FILE *fp = fopen(history_path, "r");
	if (!fp) return;

	char **lines = malloc(sizeof(char*) * MAX_HISTORY * 2);
	int count = 0;
	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp)) {
       		 lines[count] = strdup(buffer);
     	  	 count++;
        }
        fclose(fp);

        if (count <= MAX_HISTORY) {
        	for (int i = 0; i < count; i++) free(lines[i]);
        	free(lines);
        	return;
        }

        fp = fopen(history_path, "w");
        if (!fp) {
        	for (int i = 0; i < count; i++) free(lines[i]);
        	free(lines);
        	return;
        }

        int start = count - KEEP_LAST;
        if (start < 0) start = 0;

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
   	 FILE *fp = popen( "curl -s -H \"Accept: application/json\" https://icanhazdadjoke.com/ | jq -r '.joke'","r");
   	 if (!fp) {
         perror("popen");
         return;
	 }

   	 char joke[512];
   	 if (fgets(joke, sizeof(joke), fp)) {
       		 printf("\n@@ Joke of the Day:\n%s\n\n", joke);
    	} else {
		printf("\n@@ Joke of the Day:\n(No internet - could not fetch a joke.)\n\n");
	}
	 pclose(fp);
}

int is_empty_or_whitespace(const char *s) {
	while (*s) {
		if (*s != ' ' && *s != '\t' && *s != '\n') return 0;
		s++;
	}
	return 1;
}

char* read_command(){
	printf("herman >> ");
	fflush(stdout);

	char* line = NULL;
	size_t len = 0;
	getline(&line, &len, stdin);
	
	if (line) {
		line[strcspn(line, "\n")] = '\0';	
	}

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

char** parse_command(char *line) { // manage command on arry for use in execvp
	char **args = malloc(sizeof(char*) * MAX_ARGS);
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
	if (child_pid > 0) {
		kill(child_pid, SIGKILL);
	} else {
		const char *msg = "\nherman >> ";
		write(STDOUT_FILENO, msg, strlen(msg));
	}
}

void handle_sigchld(int sig) {
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_sigtstp(int sig) {
	if (child_pid > 0) {
		kill(child_pid, SIGTSTP);
		printf ("\n[1]+ stopped (pid=%d)\n", child_pid);
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
    if (job_count >= MAX_JOBS) return -1;
    jobs[job_count].pid = pid;
    strncpy(jobs[job_count].command, cmd ? cmd : "", sizeof(jobs[job_count].command)-1);
    jobs[job_count].command[sizeof(jobs[job_count].command)-1] = '\0';
    jobs[job_count].is_running = running;
    return job_count++;
}

static int find_last_stopped_job(void) {
    for (int i = job_count-1; i >= 0; --i) {
        if (jobs[i].pid > 0 && jobs[i].is_running == 0) return i;
    }
    return -1;
}

static int find_job_by_pid(pid_t pid) {
    for (int i=0;i<job_count;i++) if (jobs[i].pid == pid) return i;
    return -1;
}

static void remove_job_by_pid(pid_t pid) {
    int i = find_job_by_pid(pid);
    if (i < 0) return;
    for (int k=i+1;k<job_count;k++) jobs[k-1]=jobs[k];
    job_count--;
}


int main() 
{	
	const char* home = getenv("HOME");
	if (home == NULL) {
		fprintf(stderr, "HOME not set\n");
		exit(1);	
	}	

	char dir_path[1024];
	snprintf(dir_path, sizeof(dir_path), "%s/.herman", home);

	//char file_path[1024]; local variable not work here.
	snprintf(file_path, sizeof(file_path), "%s/history", dir_path);

	struct stat st = {0};
	if (stat(dir_path, &st) == -1) {
		mkdir(dir_path, 0700);
	}

	FILE *fp = fopen(file_path, "a");
	if (!fp) {
		perror("Cannot creat history file");
		exit(1);
	}
	fclose(fp);

	struct sigaction sa; // sa -> for sigint
	sa.sa_handler = &handle_sigint;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;	
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
	sa3.sa_flags = SA_RESTART;
	if (sigaction(SIGTSTP, &sa3, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	print_random_joke();
	
	while (1) {
		char* line = read_command();	

		char **my_command = parse_command(line);

		if (my_command[0] == NULL) {
			free(my_command);
			free(line);
			continue;
		}

		if (strcmp(my_command[0], "pwd") == 0) { //for pwd. actually this diff between addres of my_command and pwd
			char cwd[1024];
			if (getcwd(cwd, sizeof(cwd)) != NULL) { //this func give us current path 
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
				while (fgets(buffer, sizeof(buffer), fp)){
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
					if (my_command[i + 1] == NULL) {
						printf(" ");
					}
					printf("\n");
				} 
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

				
		int redirect_index = -1; //redirection for >
		for (int i = 0; my_command[i] != NULL; i++) {
			if (strcmp(my_command[i], ">") == 0) {
				redirect_index = i;
				break;
			}
		}

		int fd[2];
		if (redirect_index == -1) {
			if (pipe(fd) == -1) {
				perror("pipe");
				exit(EXIT_FAILURE);
			}
		}

		pid_t pid1 = fork();
		if (pid1 < 0) {
			perror("fork\n");
			exit(EXIT_FAILURE);
		}

		if (pid1 == 0) {
			//child proces

         	        if (redirect_index != -1) {
        			  if (my_command[redirect_index + 1] == NULL) {
		               	  	   fprintf(stderr, "Error: No file specified for redirection\n");
					   exit(1);
          			  }
       			 char *filename = my_command[redirect_index + 1];
       			 int fd_redirect = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
       			 if (fd_redirect < 0) {
           			 perror("open");
           			 exit(1);
       			 }
       			 my_command[redirect_index] = NULL;
       			 if (dup2(fd_redirect, STDOUT_FILENO) < 0) {
           			 perror("dup2");
           			 close(fd_redirect);
           			 exit(1);
       			 }
       			 close(fd_redirect);
       			 execvp(my_command[0], my_command);
       			 perror("execvp");
       			 exit(1);
   			 } else {
       				 close(fd[0]);
       				 dup2(fd[1], STDOUT_FILENO);
       				 close(fd[1]);
       				 if (strcmp(my_command[0], "ls") == 0 && my_command[1] == NULL) {
           				 char *args[] = {"ls", "--color=always", "-C", NULL};
           				 execvp(args[0], args);
       			 } else {
           			 execvp(my_command[0], my_command);
       				 }
       			 perror("exec failed");
       			 exit(EXIT_FAILURE);
   			 }

		} else {
			//parent procces

			close(fd[1]);
			char buffer[4096];
			ssize_t n;
			bool got_output = false;

			while ((n = read(fd[0], buffer, sizeof(buffer) - 1)) > 0) {
				buffer[n] = '\0';
				printf("%s", buffer);
				got_output = true;
			}
			close(fd[0]);
			
			wait(NULL);
			if (!got_output && redirect_index == -1) {
				printf("There is no such a file or directory\n");
			}

	       	}


		free(my_command);
		free(line);
	}

		return 0;
}
