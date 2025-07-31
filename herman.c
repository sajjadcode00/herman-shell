#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>


#define MAX_ARGS 1024

const char *history_file = "/home/sajjad/Desktop/linux programing/mini Shell/.herman_history";

char* read_command(){
	printf("herman >> ");
	fflush(stdout);

	char* line = NULL;
	size_t len = 0;
	getline(&line, &len, stdin);
	
	FILE *fp = fopen(history_file, "a");
	if (fp) {
		fprintf(fp, "%s", line);
		fclose(fp);
	}

	return line;
	free(line);
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
	while (1) {
		char* line = read_command();	

		char **my_command = parse_command(line);

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

	}

		return 0;
}
