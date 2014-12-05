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
