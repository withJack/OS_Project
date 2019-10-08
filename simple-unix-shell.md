# Simple Unix Shell  
#### Lee Seungjae 
#### 2014004948 Hanyang Univ. CSE

## How to compile & Execute
>Compile: 
```.../proj_shell$ make```  


>Execute: 
>* interactive mode:  ```.../proj_shell$ ./bin/shell```
>* batch mode: ```.../proj_shell$ ./bin/shell batchfile```  


## Functions Explanation
```c
char *GetCommand(void);
char **SplitCommandBySemiColon(char *command);
char **SplitCommandBySpace(char *command);
void ExecuteCommand(char **command);
int main(int argc, char *argv[]);
```

#### 1. ```char *GetCommand(void)```  
Function that gets command from standard input.  
>```fgets(buf, 1024, stdin);```  
>Use ```fgets()``` to read commands line by line.  

>```if(buf[0] == '\n') return buf;```  
>This exception catch if there is no command but '\n'.  

>``` buf[strlen(buf) -1] = '\0';```  
>Delete \n at the end of command line and set the end of array.  


#### 2. ```char **SplitCommandBySemiColon(char *command)```  
Function to split different commands by semicolon.  

>```
>split = strtok(command, delimiter);
>	while(split != NULL)
>	{
>		splits[counter++] = split;
>		split = strtok(NULL, delimiter);
>	}
>	splits[counter] = NULL;
>```
>Use ```strtok()``` to split the commands by semicolon(delimiter),  
>and save each commands in char ** variable (splits).  
>```splits[counter] = NULL``` is needed to make the last token have its end array sign.  


#### 3. ```char **SplitCommandBySpace(char *command)```  
Function to split a command by space.  
This function is used in ```execvp()``` system call in ```ExecuteCommand()``` function.  

>```
>split = strtok(command, delimiter);
>	while(split != NULL)
>	{
>		splits[counter++] = split;
>		split = strtok(NULL, delimiter);
>	}
>	splits[counter] = NULL;
>```
>Very similar to ```SplitCommandBySemiColon()``` function.  
>It saves the tokenized parts of a command into char ** variable (splits).  


#### 4. ```void ExecuteCommand(char **command)```  
Function to execute commands using ```execvp()``` system call.  

>```
>while((tmp_command = command[position++]) != NULL)
>    {
>   	pid = fork();
>      
> 	if(pid < 0)
>	    	perror("fork");
>       
>        execute_command = SplitCommandBySpace(tmp_command);
>   
>        if(pid == 0)
>        {
>            if(execvp(execute_command[0],execute_command) == -1)
>                perror("execvp");
>        }
>    	else
>            continue;
>    }
>```
>```while((tmp_command = command[position++]) != NULL)```  
>This checks if there is more commands to execute and save each command in char * variable (tmp_command) before execution.  
>Use `fork()` system call to execute the input commands on child processes.  
>Child process call `execvp()` system call to execute the command.  
>Parent process checks if there are any more commands to execute.  
>If then, go back to `while` loop to `fork()` again.

>```
>while( wait(&status) > 0 );
>
>```
>Parent process waits for all the child process to end.  
>After there is no more child process, parent process ends the function call.


#### 5. ```int main(int argc, char *argv[])```  
main function.
>```
>if(argc == 2)
>        freopen( argv[1] , "r", stdin);
>```
>If there are 2 arguments when execute the simple shell program, it is batch mode.  
>If then, we have to open batchfile and make it standard input.

>```
>while(1)
>    {
>        if(argc != 2)
>        {
>            fprintf(stdout,"prompt> ");
>            cmd_full_line = GetCommand();
>        }
>```
>For interactive mode, continuously print "prompt> " to standard output and then get commands from standard input.
>```  
>        else
>        {
>            cmd_full_line = GetCommand();
>            fprintf(stdout, "%s\n", cmd_full_line);  
>        }
>```
>If it is batch mode, get command from the file first, then print the commands before execution.  
>```           
>        if(cmd_full_line[0] == '\n')
>            continue;
>```
>If there is only \n on command line, start over.
>```
>       if(cmd_full_line[0] == '\0')
>            return 0;
>        else if(strcmp(cmd_full_line, "quit") == 0 || strcmp(cmd_full_line, "QUIT") == 0)
>            break;
>```
>Either "quit", "QUIT", CTRL-D(end of file) is entered, stop shell program.
>```
>        //tokenize with ';' to split different commands
>        each_cmd_array = SplitCommandBySemiColon(cmd_full_line);
>        
>        //execute commands
>        ExecuteCommand(each_cmd_array);
>    }
>```
>Call functions that were declared before to complete the shell tasks.

## Idea of Shell  
### Interactive Mode  
* User types command to the prompt.
* Shell program creates child process.
* Child process execute the command.

### Batch Mode
* Same mechanism with interactive mode.
* Recieve commands through file data(I/O).

