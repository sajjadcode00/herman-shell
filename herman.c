#include <stdio.h>
#include <stdlib.h> 
#include <string.h>


#define MAX_ARGS 64

char* read_command(){
	printf("herman> ");
	char* line = NULL;
	size_t len = 0;
	getline(&line, &len, stdin);
	return line;
	free(line);
}

char** parse_command(char *line) {
	char **args = malloc(sizeof(char*) * MAX_ARGS);
	int i = 0;
	char *token = strtok(line, " \t\n");
	while (token != NULL) {
		args[i++] = token;
		token = strtok(NULL, " \t\n");
	}
	args[i] = NULL;
}


int main() 
{
	while (1) {
		char* line = read_command();	
		printf ("You entered: %s", line);	
	}
	return 0;
}
