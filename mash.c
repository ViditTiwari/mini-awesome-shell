#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>


#define TRUE 1
#define FALSE !TRUE

#define BUFFER_MAX_LENGTH 50
static char* currentDirectory;
static char userInput = '\0';
static char buffer[BUFFER_MAX_LENGTH];
static int bufferChars = 0;

static char *commandArgv[5];
static int commandArgc = 0;

static pid_t MSH_PID;
static pid_t MSH_PGID;
static int MSH_TERMINAL, MSH_IS_INTERACTIVE;
static struct termios MSH_TMODES;

void getTextLine();

void populateCommand();

void destroyCommand();
void welcomeScreen();

void shellPrompt();
void handleUserCommand();

int checkBuiltInCommands();

void executeCommand(char *command[], char *file, int newDescriptor,
                    int executionMode);
void changeDirectory();

void init();



void handleUserCommand()
{
    if (checkBuiltInCommands() == 0) {
        
        launchJob(commandArgv, "STANDARD", 0, FOREGROUND);
                              //puts(commandArgv[1]);
        }
}

int checkBuiltInCommands()
{
        if (strcmp("exit", commandArgv[0]) == 0) {
                exit(EXIT_SUCCESS);
        }
        if (strcmp("cd", commandArgv[0]) == 0) {

                changeDirectory();
                return 1;
        }
       /* if (strcmp("in", commandArgv[0]) == 0) {
                launchJob(commandArgv + 2, *(commandArgv + 1), STDIN, FOREGROUND);
                return 1;
        }
        if (strcmp("out", commandArgv[0]) == 0) {
                launchJob(commandArgv + 2, *(commandArgv + 1), STDOUT, FOREGROUND);
                return 1;
        }*/
        if (strcmp("bg", commandArgv[0]) == 0) {
                if (commandArgv[1] == NULL)
                        return 0;
                if (strcmp("in", commandArgv[1]) == 0)
                        launchJob(commandArgv + 3, *(commandArgv + 2), STDIN, BACKGROUND);
                else if (strcmp("out", commandArgv[1]) == 0)
                        launchJob(commandArgv + 3, *(commandArgv + 2), STDOUT, BACKGROUND);
                else
                        launchJob(commandArgv + 1, "STANDARD", 0, BACKGROUND);
                return 1;
        }
        if (strcmp("fg", commandArgv[0]) == 0) {
                if (commandArgv[1] == NULL)
                        return 0;
                int jobId = (int) atoi(commandArgv[1]);
                t_job* job = getJob(jobId, BY_JOB_ID);
                if (job == NULL)
                        return 0;
                if (job->status == SUSPENDED || job->status == WAITING_INPUT)
                        putJobForeground(job, TRUE);
                else                                                                                                // status = BACKGROUND
                        putJobForeground(job, FALSE);
                return 1;
        }
        if (strcmp("jobs", commandArgv[0]) == 0) {
                printJobs();
                return 1;
        }
        if (strcmp("kill", commandArgv[0]) == 0)
        {
                if (commandArgv[1] == NULL)
                        return 0;
                killJob(atoi(commandArgv[1]));
                return 1;
        }
        //int i;
        ////for( i=0;i<=commandArgc;i++)
        //{
        //char *str;
        //strcpy(str,commandArgv);
        if( commandArgc == 3 )
        {
        if(strcmp("|", commandArgv[1]) == 0  )
            {
            pipelining(2);
            return 1;
            }
            //return 1;
        }
    if( commandArgc > 3 )
{
        if(strcmp("|", commandArgv[commandArgc-2]) == 0 )
        {
            //puts("works");
            //printf("%d",commandArgc);
            pipelining(commandArgc-1);
            return 1;
        }
    }
        //break;
        //}



        return 0;
}

void executeCommand(char *command[], char *file, int newDescriptor,
                    int executionMode)
{
        int commandDescriptor;
        if (newDescriptor == STDIN) {
                commandDescriptor = open(file, O_RDONLY, 0600);
                dup2(commandDescriptor, STDIN_FILENO);
                close(commandDescriptor);
        }
        if (newDescriptor == STDOUT) {
                commandDescriptor = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0600);
                dup2(commandDescriptor, STDOUT_FILENO);
                close(commandDescriptor);
        }
        if (execvp(*command, command) == -1)
                perror("MSH");
}

void changeDirectory()
{
        if (commandArgv[1] == NULL) {
                chdir(getenv("HOME"));
        } else {
                if (chdir(commandArgv[1]) == -1) {
                        printf(" %s: no such directory\n", commandArgv[1]);
                }
        }
}

void getTextLine()
{
        destroyCommand();
        while ((userInput != '\n') && (bufferChars < BUFFER_MAX_LENGTH)) {
                buffer[bufferChars++] = userInput;
                userInput = getchar();
        }
        buffer[bufferChars] = 0x00;
        populateCommand();
}
void populateCommand()
{
        char* bufferPointer;
        bufferPointer = strtok(buffer, " ");
        while (bufferPointer != NULL) {
                commandArgv[commandArgc] = bufferPointer;
                bufferPointer = strtok(NULL, " ");
                commandArgc++;
        }
}

void destroyCommand()
{
        while (commandArgc != 0) {
                commandArgv[commandArgc] = NULL;
                commandArgc--;
        }
        bufferChars = 0;
}

void welcomeScreen()
{
        printf("\n-------------------------------------------------\n");
        printf("\tWelcome to mini-AWESOME-shell version \n");
        printf("-------------------------------------------------\n");
        printf("\n\n");
}

void shellPrompt()
{
        printf("%s :> ",getcwd(currentDirectory, 1024));
}

void init()
{
        MSH_PID = getpid();
        MSH_TERMINAL = STDIN_FILENO;
        MSH_IS_INTERACTIVE = isatty(MSH_TERMINAL);

        if (MSH_IS_INTERACTIVE) {
                while (tcgetpgrp(MSH_TERMINAL) != (MSH_PGID = getpgrp()))
                        kill(MSH_PID, SIGTTIN);

                signal(SIGQUIT, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTSTP, SIG_IGN);
                signal(SIGINT, SIG_IGN);
                signal(SIGCHLD, &signalHandler_child);

                setpgid(MSH_PID, MSH_PID);
                MSH_PGID = getpgrp();
                if (MSH_PID != MSH_PGID) {
                        printf("Error, the shell is not process group leader");
                        exit(EXIT_FAILURE);
                }
                if (tcsetpgrp(MSH_TERMINAL, MSH_PGID) == -1)
                        tcgetattr(MSH_TERMINAL, &MSH_TMODES);

                currentDirectory = (char*) calloc(1024, sizeof(char));
        } else {
                printf("Could not make MSH interactive. Exiting..\n");
                exit(EXIT_FAILURE);
        }
}

int main(int argc, char **argv, char **envp)
{
        init();
        welcomeScreen();
        shellPrompt();
        while (TRUE) {
                userInput = getchar();
                switch (userInput) {
                case '\n':
                        shellPrompt();
                        break;
                default:
                        getTextLine();
                        handleUserCommand();
                        shellPrompt();
                        break;
                }
        }
        printf("\n");
        return 0;
}
