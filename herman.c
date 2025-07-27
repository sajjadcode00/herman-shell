#include <stdio.h>
#include <stdlib.h> 
#include <string.h>


char* read_command(){
	printf("herman> ");
	char* line = NULL;
	size_t len = 0;
	getline(&line, &len, stdin);
	return line;
	free(line);
}
int main() 
{
	while (1) {
		char* line = read_command();	
		printf ("You enreed: %s", line);
		line[strcspn(line, "\n")] = '\0';	
	}
	return 0;
}
