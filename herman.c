#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>


#define MAX_ARGS 1024
#define MAX_HISTORY 500
#define KEEP_LAST 10

char file_path[1024]; // it must be global variable. because I use it in read_command().

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
				while (fgets(buffer, sizeof(buffer), fp)) {
					printf("%d %s", count++, buffer);
				}
				fclose(fp);
			} else {
				perror("history");
			}
			continue;
		}

		int fd[2];
		if (pipe(fd) == -1) { //this is for ls
			perror("pipe\n");
			exit(EXIT_FAILURE);
		}

		pid_t pid1 = fork();
		if (pid1 < 0) {
			perror("fork\n");
			exit(EXIT_FAILURE);
		}

		if (pid1 == 0) {
			//child proces
			close(fd[0]);
			dup2(fd[1], STDOUT_FILENO);
			close(fd[1]);

			if (strcmp(my_command[0], "ls") == 0 && my_command[1] == NULL) {
				char *args[] = {"ls", "--color=always", "-C", NULL};
				execvp(args[0], args);
			} else {
				execvp(my_command[0], my_command);
			}

			perror("exec failed\n");
			exit(EXIT_FAILURE);
		} else {
			//parent procces
			close(fd[1]);
			char buffer[4096];
			int n = read(fd[0], buffer, sizeof(buffer) - 1);
			buffer[n] = '\0';
			close(fd[0]);

			wait(NULL);

			if (n == 0) {printf("There is no file or directory\n");}
			else {printf("%s", buffer);}
	       	}


		free(my_command);
		free(line);
	}

		return 0;
}
