#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[]){
	struct stat statbuf;
	FILE *fp;
	char buffer;
	long long len = strlen(argv[1]);
	long long tag = 0;
	if(argc == 2){
		while((buffer = fgetc(stdin)) != EOF){		
			if(buffer == '\n'){
				fprintf(stdout, "%lld\n", tag);
				tag = 0;
			}
			else{
				for(int i = 0; i < len; i++)
					if(buffer == argv[1][i]){
						tag++;
						break;
					}
			}	
		}			
	}
	else if(argc == 3){
		fp = fopen(argv[2], "r");
		if(fp){
			stat(argv[2], &statbuf);
			if(S_ISDIR(statbuf.st_mode))
				fprintf(stderr, "error\n");					
			while((buffer = fgetc(fp)) != EOF){		
				if(buffer == '\n'){
					fprintf(stdout, "%lld\n", tag);
					tag = 0;
				}
				else{
					for(int i = 0; i < len; i++)
						if(buffer == argv[1][i]){
							tag++;
							break;
						}
				}	
			}		
			fclose(fp);
		}
		else
			fprintf(stderr, "error\n");		
	}
	return 0;
}
