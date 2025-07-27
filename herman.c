#include <stdio.h>
#include <stdlib.h> 

int main() 
{
	while(1) {
		printf("herman> ");
		char* line = NULL;
		size_t len = 0;
		getline(&line, &len, stdin);
		printf("You entered: %s", line);
		free(line);
	}

	return 0;
}
