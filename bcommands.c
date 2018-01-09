#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <string.h>
#include "parse.h"
#include "parse_Cmd.h"

static char *builtin[] = {"cd", "echo", "logout", "pwd", "setenv", "unsetenv", "where", NULL};
extern char **environ;

void setup_args(Cmd c, int status) {
	char **new_args = malloc(c->maxargs*sizeof(char*));
	int i = 1;
	if(status == 0) {
		while(c->args[i]!=NULL) {
			new_args[i-1] = c->args[i];
			i++;
		}
		new_args[i] = NULL;
		c->args = new_args;
		--c->nargs;
	} else if (status == 1) {
		while(c->args[i+1]!=NULL) {
			new_args[i-1] = c->args[i+1];
			i++;
		}
		new_args[i-1] = NULL;
		c->args = new_args;
		c->nargs = c->nargs - 2;
	}
	prCmd(c);
}

int ush_nice(Cmd c) {
	int a;
	int pid;
	if(!strcmp(c->args[0],"nice")) {
    	//printf("c->args[0] = %s, c->args[1] = %s, c->args[2] = %s\n", c->args[0], c->args[1], c->args[2]);
        // no value or command specified
        if(c->nargs == 1) {
        	pid = getpid();
            //printf("no arguments\n");
            if(setpriority(PRIO_PROCESS, pid, 4)  == -1) {
            	perror("setting priority of the current process failed\n");
				return -1;
            }
			//c->args[0] = "already run";
        } else if(c->nargs == 2 && c->args[1][0] == '+') {
          	a = atoi(&c->args[1][1]);
            printf("nice number = %d\n", a);
			pid = getpid();
            if(setpriority(PRIO_PROCESS, pid, a) == -1) {
            	perror("failed to set nice\n");
                return -1; // Just for now
            }
        } else if(c->nargs > 1) {
        	if(c->args[1][0] != '+') {           //no nice number, just the command
				pid = getpid();
            	if(setpriority(PRIO_PROCESS, pid, 4) == -1) {
                	perror("nice(4) failed\n");
                    return -1;
                }
				setup_args(c, 0);
            } else if(c->args[1][0] == '+' && c->nargs >= 3) {
				pid = getpid();
            	a = atoi(&c->args[1][1]);
                if(setpriority(PRIO_PROCESS, pid, a) == -1) {
                	perror("failed to set nice");
					return -1;
                }
                /*c->args[0] = c->args[2];
                if(c->args[3] == NULL) {
                	c->args[1] = NULL;
                    c->args[2] = NULL;
                }
                free(c->args[0]);*/
				setup_args(c, 1);
            }
		}
	}
}

/*char* strtoken(char *p, int start, int *index) {
	if(start > (strlen(p) - 1) ) {
		return NULL;
	}
	int i = start;

	while(p[i] != ':' && p[i] != '\0') i++;

	char *dest = (char*)malloc( (i+1) - start);
	i = start;
	int j = 0;
	while(p[i] != ':' && p[i] != '\0') {
		dest[j++] = p[i++];
	}
	*index = i+1;
	dest[j] = '\0';
	return dest;
}*/


void ush_where(char *command) {
	char **i = builtin;
	char *path, *dir, *delim;
	char **result = NULL;   //list of paths where the file is found
	int j = 0;       //Index to the result
	FILE *fp = NULL;
	int index = 0, sub_index = 0;
	char *dest = (char *) malloc(256*sizeof(char));
	int start =0, restart = 0;
	//struct stat st;
	if(command == NULL) {
		printf("Usage: where <command>\n");
		return;
	}

	while(*i) {
		if(strcmp(command, *i) == 0) {
			write(0,"Shell builtin command\n", 23);
		}
		i++;
	}
	/* Not a builtin command. Search in dirs pointed by PATH */
	path = (char *) malloc(strlen(getenv("PATH")));
	strcpy(path,getenv("PATH"));
	//printf("path = %s\n", path);
	delim = ":";
	dir = strtok(path, delim);
	while(dir!=NULL) {
		//ush_change_dir(dir);
		while(dir[index]!='\0') {
			dest[index] = dir[index];
			index++;
		}
		dest[index++] = '/';
		while(command[sub_index]!='\0') {
			dest[index] = command[sub_index];
			index++;
			sub_index++;
		}
		dest[index] = '\0';
		//printf("dest = %s\n", dest);
		/* Search if a file matching the command exists */
		fp = fopen(dest, "r");
		if(fp!=NULL) {
			printf("Found in %s \n", dest);
			j++;
			fclose(fp);
		}
		//start = restart;
		dir = strtok(NULL, delim);
		index = 0;
		sub_index = 0;
		//fclose(fp);
	}
	free(dir);
	free(dest);
	//fclose(fp);
}

void err_env() {
	if(errno == EINVAL) {
		perror("Invalid parameter <name> of length 0\n");
        return;
	} else if(errno == ENOMEM) {
		perror("No memory available to set the env variable\n");
		return;
    }
}

void ush_setenv(char *name, char *value, int nargs) {
	int i;
	if(nargs == 1) {
		for(i=0; environ[i]; i++) {
			printf("%s\n", environ[i]);
		}
		return;
	}
	if(value == NULL) {
		if(setenv(name, NULL,0) == -1) {
			err_env();
			return;
		}
		return;
	} 
	if(getenv(name)) {
		if(setenv(name, value, 1) == -1) {
			err_env();
			return;
		}
	} /*else {
		if((setenv(name, value, 0) == -1)) {
			err_env();
		}
		return;
	}*/
}

void ush_unsetenv(char *name) {
	if(unsetenv(name) == -1) {
		err_env();
	}
}

//change the directory
void ush_change_dir(char *path) {
	if(chdir(path) == -1) {
		if(errno == ENOENT) {
			perror("No such file or directory\n");
			return;
		} else if(errno == EACCES) {
			perror("Permission Denied. Should be root to perform this operation\n");
			return;
		}	
	}
}

void ush_present_dir() {
	char *buf = (char *)get_current_dir_name();
	if(buf == NULL) {
		/* check the errno */
		if(errno == EACCES) {
			perror("Permission Denied. Should be root to perform this operation\n");
			return;
		}
	}
	printf("%s",buf);
	printf("\n");
}

void ush_logout() {
	exit(0);
}

void ush_echo(Cmd c) {
	if(c->nargs<2) {
		return;
	}

	int i = 1;
	while(i < c->nargs) {
		fprintf(stdout,"%s\n", c->args[i]);
		i++;
	}
	//printf("\n");
	
	/* check each argument for the presence of '$'
	 * handle it accordingly
	 */
	/* I know this works but commenting out for non-obvious reasons*/
	/*char *command = NULL, *env_var = NULL, *str = NULL;
	char *temp;
	char *ptr;
	char *delim = "$";
	int arg_count = c->nargs;
	int i = 0,j=0;

	temp = (char *) malloc(256 * sizeof(char));
	command = (char *) malloc(256 * sizeof(char));

	while(arg_count>1) {
		strcpy(temp, c->args[i+1]);
		ptr = strchr(temp, '$');
		if(ptr!=NULL) {
			while(temp[j]!='$') {
				command[j] = temp[j];
				j++;
			}
			command[j] = '\0';
			if(j>0) {
				printf("%s", command);
			} //else {
				str = strtok(c->args[i+1], delim);
				while(str!=NULL) {
					env_var = getenv(str);
					if(env_var!=NULL) {
                		printf("%s", env_var);
            		} else {
                		// no-op
            		}
					str = strtok(NULL, "$");
				}
			//}
		} else {
			printf("%s", temp);
		}

		j = 0;
		i++;
		arg_count--;
		printf(" ");
		free(command);
		command = (char *) malloc(256 * sizeof(char)); 
	}
	printf("\n");*/
}
