#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "parse.h"
#include "parse_Cmd.h"

//int filedes[2];
int fd[3];
int std_in, std_out, std_err;
static int eof;
int handler = 0;

int redirection(Cmd c) {
  	//Check if the input to the command is from a pipe
  	if(c->in == Tpipe || c->in == TpipeErr) {
    	close(0);			// Close the current input file

  		if(dup2(c->filedes[0], 0) ==  -1) {
			//errno = 0; 
        	perror("Error in dup2()");
      		return -1;
    	}
		//close(c->filedes[0]);
  	} else if(c->in == Tin) { //Indicates that input file has been specified
    	//Open the file indicated to read the input from
    	//
    	if((std_in = open((const char*) c->infile, O_RDONLY)) == -1) {
			fprintf(stdout, "%s: No such file or directory\n", c->infile);
			//perror("file cannot be opened or no such file exists\n");
			return -1;
    	}
    	close(0);
    	if(dup(std_in) ==  -1) { 
        	perror("Error in dup2()\n");
      		return -1;
    	}
  	}

  	if(c->next != NULL && c->next->in == Tpipe) {
    	if(pipe(c->next->filedes) == -1) {
        	perror("Pipe failed\n");
      		return -1;
    	}

    	close(1);
    	if(dup2(c->next->filedes[1], 1) == -1) {
      		perror("Error in dup2\n");
      		return -1;
    	}

  	}
  	if(c->next != NULL && c->next->in == TpipeErr) {
    	if(pipe(c->next->filedes) == -1) {
        	perror("Pipe failed\n");
      		return -1;
    	}

    	close(1);
    	if(dup2(c->next->filedes[1], 1) == -1) {
      		perror("Error in dup2\n");
      		return -1;
    	}
    	close(2);
   		if(dup2(c->next->filedes[1], 2) == -1) {
      		perror("Error in dup2()\n");
      		return -1;
    	}
	} else if(c->out != Tnil) { 
		//Indicates that the output file has to be redirected
    	//Open the output file to which the redirection has to be done
    	//Open the file for write or append
    	if(c->out == Tout || c->out == ToutErr) {
      		if((std_out = open((char*) c->outfile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1) {
				return -1;
        	}
    	}
    	else if(c->out == Tapp || c->out == TappErr) {
      		if((std_out = open((char*) c->outfile, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) == -1) {
          		return -1;
      		}
    	}
    	close(1);
    	if(dup2(std_out, 1) == -1) {
      		perror("Error in dup2()\n");
      		return -1;
    	}

    	//Redirect the std err
    	if(c->out == ToutErr || c->out == TappErr) {
      		std_err = std_out;
      		close(2);
      		if(dup2(std_err, 2) == -1) {
        		perror("Error in dup2()");
        		return -1;
      		}
    	}
	}
  	return 1;  
}

void orig_fd_state() {
	fd[0] = dup(0);
	fd[1] = dup(1);
	fd[2] = dup(2);
}

void revert_orig_state() {
	close(0);
	dup(fd[0]);
	close(1);
	dup(fd[1]);
	close(2);
	dup(fd[2]);
}

void parent_pipe_reset(Cmd c) {
	if(c->in == Tpipe) {
		close(c->filedes[0]);
	}
	if(c->next!=NULL && (c->next->in==Tpipe || c->next->in==TpipeErr)) {
		close(c->next->filedes[1]);
		if(c->next->in == TpipeErr) {
			close(c->next->filedes[1]);
		}
	}
	revert_orig_state();
}


void prCmd(Cmd c)
{
	int i;
	int process_id, status;
	int child_fd;

  	//if (c) {
		if(!strcmp(c->args[0],"nice")) {
			ush_nice(c);
			//prCmd(c);
			return;
		}
		if(!strcmp(c->args[0],"where")) {
			ush_where(c->args[1]);
			return;
		} else if(!strcmp(c->args[0],"echo")) {
			int arg_count = c->nargs;
        	ush_echo(c);
    	} else if(!strcmp(c->args[0],"setenv")) {
        	ush_setenv(c->args[1], c->args[2], c->nargs);
        	return;
    	} else if(!strcmp(c->args[0],"unsetenv")) {
			ush_unsetenv(c->args[1]);
			return;
		} else if(!strcmp(c->args[0],"cd")) {
        	ush_change_dir(c->args[1]);
        	return;
    	} else if(!strcmp(c->args[0],"pwd")) {
        	ush_present_dir(c->args[1]);
        	return;
    	} else if (!strcmp(c->args[0], "logout") || !strcmp(c->args[0], "exit") || !strcmp(c->args[0], "quit")) {
      		exit(0);
		} else {
			//It is a system command (can be a junk too!)
			redirection(c);
			if((child_fd = fork()) == -1) {
				perror("forking a new subshell failed\n");
			}
			if(child_fd == 0) {
				//sleep(30);
				signal(SIGINT, SIG_DFL);
				//close(c->filedes[0]);
				//close(c->filedes[1]);
                if(execvp(c->args[0], c->args) == -1) {
					if(errno == EACCES) {
						fprintf(stdout, "Permission Denied");
					} else if (errno == ENOENT) {
						fprintf(stdout, "File does not exist");
					} else if(errno == EISDIR) {
						fprintf(stdout, "%s is a directory", c->args[0]);
					}
					//errno = 0;
				 	//kill(-2,SIGTERM);
				 	fflush(stdout);
					fflush(stdin);
					abort();
				}
			} else {
				//close(c->filedes[1]);
				//process_id = waitpid(child_fd,&status, 0);
				//sleep(1);
				parent_pipe_reset(c);    //No need for the parent to modify its std fd's
				/*while(wait(&status) == -1) {
					if(status!=0)
						kill(0, SIGINT);
    			}*/
				//revert_orig_state();
			}
		}
}

static int prPipe(Pipe p)
{
	int i, j;
	int child_fd;
	int process_id;
	int rd_status;
	int status;
  	Cmd c;
	int err = 0;

  	if (p == NULL)
    	return;

  	for (c = p->head; c != NULL; c = c->next) {
		if(!strcmp(c->args[0], "end")) {
			eof = 1;
			return 0;
		}
	
		// Check if the commands are in a pipe
		if(c->next!=NULL) {
			err = c->next->in == TpipeErr? 1:0;
			rd_status = redirection(c);
			if(rd_status == -1) {
				//perror("setting up pipe handlers failed\n");
				return -1;
			}

			if(!strcmp(c->args[0],"nice")) {
				if((child_fd = fork()) == -1) {
					perror("forking a new subshell failed\n");
					return -1;
				}
				if(child_fd == 0) {
					signal(SIGINT, SIG_DFL);
					close(c->next->filedes[0]);
					close(c->next->filedes[1]);
                    ush_nice(c);
					exit(0);       // Should call exit to kill the forked ush!
				}
			}else if(!strcmp(c->args[0],"where")) {
                if((child_fd = fork()) == -1) {
                    perror("forking a new subshell failed\n");
					return -1;
                }
                if(child_fd == 0) {      //Execute the child
					signal(SIGINT, SIG_DFL);
					close(c->next->filedes[0]);
					close(c->next->filedes[1]);
                    ush_where(c->args[1]);
					exit(0);       // Should call exit to kill the forked ush!
                } else {
					parent_pipe_reset(c);
				}
            } else if(!strcmp(c->args[0],"echo")) {
                if((child_fd = fork()) == -1) {
                    perror("forking a new subshell failed\n");
					return -1;
                }
                if(child_fd == 0) {
					signal(SIGINT, SIG_DFL);
					close(c->next->filedes[0]);
					close(c->next->filedes[1]);
                    ush_echo(c);
					exit(0);
                } else {
					parent_pipe_reset(c);
				}
            } else if(!strcmp(c->args[0],"setenv")) {
                if((child_fd = fork()) == -1) {
                    perror("forking a new subshell failed\n");
					return -1;
                }
                if(child_fd == 0) {
					signal(SIGINT, SIG_DFL);
					close(c->next->filedes[0]);
					close(c->next->filedes[1]);
					ush_setenv(c->args[1], c->args[2], c->nargs);
					exit(0);
                } else {
					parent_pipe_reset(c);
				}
            } else if(!strcmp(c->args[0],"unsetenv")) {
                if((child_fd = fork()) == -1) {
                    perror("forking a new subshell failed\n");
					return -1;
                }
                if(child_fd == 0) {
					signal(SIGINT, SIG_DFL);
					close(c->next->filedes[0]);
					close(c->next->filedes[1]);
                    ush_unsetenv(c->args[1]);
					exit(0);
                } else {
					parent_pipe_reset(c);
				}
            } else if(!strcmp(c->args[0],"cd")) {
                if((child_fd = fork()) == -1) {
                    perror("forking a new subshell failed\n");
					return -1;
                }
                if(child_fd == 0) {
                    signal(SIGINT, SIG_DFL);
					close(c->next->filedes[0]);
					close(c->next->filedes[1]);
					ush_change_dir(c->args[1]);
					exit(0);
                } else {
					parent_pipe_reset(c);
				}
            } else if(!strcmp(c->args[0],"pwd")) {
				if((child_fd = fork()) == -1) {
                	perror("forking a new subshell failed\n");
					return -1;
				}
        		if(child_fd == 0) {
					signal(SIGINT, SIG_DFL);
					close(c->next->filedes[0]);
					close(c->next->filedes[1]);
					ush_present_dir();
					exit(0);
        		} else {
					parent_pipe_reset(c);
				}
			} else {
				// If none of the above match the criteria, then it has to be a system command
				if((child_fd = fork()) == -1) {
                    perror("forking a new subshell failed\n");
					return -1;
                }
                if(child_fd == 0) {
					signal(SIGINT,SIG_DFL);
					close(c->next->filedes[0]);
					close(c->next->filedes[1]);
					status = execvp(c->args[0], c->args);
                    if(status == -1) {
						if(errno == EACCES) {
							fprintf(stdout, "Permission Denied");
						} else if (errno == ENOENT) {
							fprintf(stdout, "File does not exist");
						} else if(errno == EISDIR) {
							fprintf(stdout, "%s is a directory", c->args[0]);
						}
						//errno = 0;
						perror("Bad command");
					 	kill(0,SIGTERM);
						//abort();
					}
                } else {
					parent_pipe_reset(c);
				} 
				if(handler) {
					handler = 0;
					return -1;
				}
			}
		} else {
    		prCmd(c);
		}
	}
	while(wait(&status)!= -1) {
		if(status!=0)
			kill(0, SIGINT);
    }
	if(handler) {
		handler = 0;
		return -1;
	}
  	prPipe(p->next);
}

void read_config() {
	Pipe p;
	char *path = (char *)malloc(256*sizeof(char));
	
	strcpy(path,getenv("HOME"));
	if(path == NULL) {
		perror("getenv failed\n");
	}
	strcat(path, "/.ushrc");
	//printf("path = %s\n", path);

	int fd = open(path, O_RDONLY);
	if(fd == -1) {
		//check the errno
		if(errno == EACCES) {
			perror("Permission Denied\n");
			return;
		}
		if(errno == EMFILE) {
            perror("Too many files open\n");
            return;
        }
		if(errno == ENAMETOOLONG) {
            perror("Pathname too long\n");
            return;
        }
		return;
	}

	//redirect the file fd to stdin for the parser
	close(0);
	int rd = dup2(fd, 0);
	if(rd == -1) {
		perror("dup2 failed\n");
		return;
	}
	
	//All is well. Parse the file
	while(1) {
		p = parse();
		prPipe(p);
		freePipe(p);
		if(eof == 1) {
			//printf("end of file\n");
			close(fd);
			free(path);
			revert_orig_state();
			eof = 0;
			return;
		}
	}
}

void sigterm_handler() {
	handler = 1;
	signal(SIGTERM, sigterm_handler);
}

void sigint_handler(int sig) {
	if(sig == SIGINT || sig == SIGTSTP) {
		printf("\n");
	}
	signal(SIGINT, sigint_handler);
	signal(SIGTSTP, sigint_handler);
	printf("\n");
}

int main(int argc, char *argv[])
{
	Pipe p;
  	char *host = (char *)malloc(256*sizeof(char *));
	if(host == NULL) {
		host = "ush";
	}else {
		strcpy(host, getenv("HOME"));
	}

	orig_fd_state();
	read_config();

  	signal(SIGINT, sigint_handler);
	signal(SIGTSTP, sigint_handler);
	signal(SIGQUIT, SIG_IGN);
  	signal(SIGTERM, sigterm_handler);

	fflush(stdout);
	fflush(stdin);

	revert_orig_state();
  	while (1) {
    	printf("%s%% ", host);
		fflush(stdout);
		fflush(stdin);
    	p = parse();
    	if(prPipe(p) == -1) {
			revert_orig_state();
			//fprintf(stdout,"Command execution failed\n");
			freePipe(p);
			continue;
		}
		revert_orig_state();
    	freePipe(p);
		if(eof == 1) return;
  	}
}
