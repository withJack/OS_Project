#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

char *GetCommand(void);
char **SplitCommandBySemiColon(char *command);
char **SplitCommandBySpace(char *command);
void ExecuteCommand(char **command);

int main(int argc, char *argv[])
{
    char* cmd_full_line;
    char** each_cmd_array;

    //batch mode file read control
    if(argc == 2)
        freopen( argv[1] , "r", stdin);

    while(1)
    {
        //interactive mode 
        if(argc != 2)
        {
            fprintf(stdout,"prompt> ");
            cmd_full_line = GetCommand();
        }
        //batch mode
        else
        {
            cmd_full_line = GetCommand();
            fprintf(stdout, "%s\n", cmd_full_line);  
        }
            
        //check if there is only \n on command line
        //if then, start over
        if(cmd_full_line[0] == '\n')
            continue;

        //quit, QUIT, CTRL-D keys to stop shell
        if(cmd_full_line[0] == '\0')
            return 0;
        else if(strcmp(cmd_full_line, "quit") == 0 || strcmp(cmd_full_line, "QUIT") == 0)
            break;

        //tokenize with ';' to split different commands
        each_cmd_array = SplitCommandBySemiColon(cmd_full_line);
        
        //execute commands
        ExecuteCommand(each_cmd_array);
    }

    return 0;
}


//function to get command line from standard input
char* GetCommand(void)
{
	char *buf = malloc(sizeof(char) * 1024);

	//exception for malloc error
	if(!buf)
	{
		fprintf(stderr, "getCommand allocation error!\n");
		exit(-1);
	}

	//get Command by line
	fgets(buf, 1024, stdin);

    //if there is only \n in command line
    if(buf[0] == '\n')
        return buf;

    //delete \n from the command line
    //and set the end
    buf[strlen(buf) -1] = '\0';

	return buf;
}


//function to split different commands by ';'
char **SplitCommandBySemiColon(char *command)
{
	char **splits = malloc(sizeof(char*)*256);
	char *split;
	char *delimiter = ";";
	int counter = 0;

	//exception for malloc error
	if(!splits)
	{
		fprintf(stderr, "splitbysemicolon allocation error!\n");
		exit(-1);
	}
	
	//split into tokens by ';'
    //save the commands to char** variable
	split = strtok(command, delimiter);
	while(split != NULL)
	{
		splits[counter++] = split;
		split = strtok(NULL, delimiter);
	}
	splits[counter] = NULL;
    
	return splits;
}


//function to split a command by space 
//used in execvp() system call in ExecuteCommand() function
char **SplitCommandBySpace(char *command)
{
	char **splits = malloc(sizeof(char*)*128);
	char *split;
	char *delimiter = " ";
	int counter = 0;

	//exception for malloc error
	if(!splits)
	{
		fprintf(stderr, "splitbyhyphen allocation error!\n");
		exit(-1);
	}
	
	//split into tokens by space
    //save each part of the command to char** variable
	split = strtok(command, delimiter);
	while(split != NULL)
	{
		splits[counter++] = split;
		split = strtok(NULL, delimiter);
	}
	splits[counter] = NULL;

    return splits;
}


//function to execute commands simultaneously 
//using execvp() system call
void ExecuteCommand(char **command)
{
	pid_t pid;
	int status, position = 0;
    char *tmp_command;
    char **execute_command;

    //check if there is further commands
    //save each command in temporary char* variable
    while((tmp_command = command[position++]) != NULL)
    {
	    //make child process to execute command
    	pid = fork();

    	//exception for fork error
    	if(pid < 0)
	    	perror("fork");
        
        //split by space
        execute_command = SplitCommandBySpace(tmp_command);

    	//child process
        if(pid == 0)
        {
            if(execvp(execute_command[0],execute_command) == -1)
                perror("execvp");
        }

    	//parent process
        //check if there is more commands
        //if then, continue fork()
    	else
            continue;
    }

    //wait for all child processes to finish
    //before print out next prompt> message
    while( wait(&status) > 0 );
}
