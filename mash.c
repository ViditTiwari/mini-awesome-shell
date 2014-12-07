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


#define FOREGROUND 'F'
#define BACKGROUND 'B'
#define SUSPENDED 'S'
#define WAITING_INPUT 'W'


#define STDIN 1
#define STDOUT 2


#define BY_PROCESS_ID 1
#define BY_JOB_ID 2
#define BY_JOB_STATUS 3

static int numActiveJobs = 0;

typedef struct job {
        int id;
        char *name;
        pid_t pid;
        pid_t pgid;
        int status;
        char *descriptor;
        struct job *next;
} t_job;

static t_job* jobsList = NULL;

static pid_t MSH_PID;
static pid_t MSH_PGID;
static int MSH_TERMINAL, MSH_IS_INTERACTIVE;
static struct termios MSH_TMODES;

void getTextLine();

void populateCommand();

void destroyCommand();

t_job * insertJob(pid_t pid, pid_t pgid, char* name, char* descriptor,
                  int status);

t_job* delJob(t_job* job);

t_job* getJob(int searchValue, int searchParameter);

void printJobs();

void welcomeScreen();

void shellPrompt();
void handleUserCommand();

int checkBuiltInCommands();

void executeCommand(char *command[], char *file, int newDescriptor,
                    int executionMode);

void launchJob(char *command[], char *file, int newDescriptor,
               int executionMode);

void putJobForeground(t_job* job, int continueJob);

void putJobBackground(t_job* job, int continueJob);

void waitJob(t_job* job);

void killJob(int jobId);

void changeDirectory();

void init();

void signalHandler_child(int p);




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

t_job* insertJob(pid_t pid, pid_t pgid, char* name, char* descriptor,
                 int status)
{
        usleep(10000);
        t_job *newJob = malloc(sizeof(t_job));

        newJob->name = (char*) malloc(sizeof(name));
        newJob->name = strcpy(newJob->name, name);
        newJob->pid = pid;
        newJob->pgid = pgid;
        newJob->status = status;
        newJob->descriptor = (char*) malloc(sizeof(descriptor));
        newJob->descriptor = strcpy(newJob->descriptor, descriptor);
        newJob->next = NULL;

        if (jobsList == NULL) {
                numActiveJobs++;
                newJob->id = numActiveJobs;
                return newJob;
        } else {
                t_job *auxNode = jobsList;
                while (auxNode->next != NULL) {
                        auxNode = auxNode->next;
                }
                newJob->id = auxNode->id + 1;
                auxNode->next = newJob;
                numActiveJobs++;
                return jobsList;
        }
}


int changeJobStatus(int pid, int status)
{
        usleep(10000);
        t_job *job = jobsList;
        if (job == NULL) {
                return 0;
        } else {
                int counter = 0;
                while (job != NULL) {
                        if (job->pid == pid) {
                                job->status = status;
                                return TRUE;
                        }
                        counter++;
                        job = job->next;
                }
                return FALSE;
        }
}

t_job* delJob(t_job* job)
{
        usleep(10000);
        if (jobsList == NULL)
                return NULL;
        t_job* currentJob;
        t_job* beforeCurrentJob;

        currentJob = jobsList->next;
        beforeCurrentJob = jobsList;

        if (beforeCurrentJob->pid == job->pid) {

                beforeCurrentJob = beforeCurrentJob->next;
                numActiveJobs--;
                return currentJob;
        }

        while (currentJob != NULL) {
                if (currentJob->pid == job->pid) {
                        numActiveJobs--;
                        beforeCurrentJob->next = currentJob->next;
                }
                beforeCurrentJob = currentJob;
                currentJob = currentJob->next;
        }
        return jobsList;
}

t_job* getJob(int searchValue, int searchParameter)
{
        usleep(10000);
        t_job* job = jobsList;
        switch (searchParameter) {
        case BY_PROCESS_ID:
                while (job != NULL) {
                        if (job->pid == searchValue)
                                return job;
                        else
                                job = job->next;
                }
                break;
        case BY_JOB_ID:
                while (job != NULL) {
                        if (job->id == searchValue)
                                return job;
                        else
                                job = job->next;
                }
                break;
        case BY_JOB_STATUS:
                while (job != NULL) {
                        if (job->status == searchValue)
                                return job;
                        else
                                job = job->next;
                }
                break;
        default:
                return NULL;
                break;
        }
        return NULL;
}

void printJobs()
{
        printf("\nActive jobs:\n");
        printf(
                "---------------------------------------------------------------------------\n");
        printf("| %7s  | %30s | %5s | %10s | %6s |\n", "job no.", "name", "pid",
               "descriptor", "status");
        printf(
                "---------------------------------------------------------------------------\n");
        t_job* job = jobsList;
        if (job == NULL) {
                printf("| %s %62s |\n", "No Jobs.", "");
        } else {
                while (job != NULL) {
                        printf("|  %7d | %30s | %5d | %10s | %6c |\n", job->id, job->name,
                               job->pid, job->descriptor, job->status);
                        job = job->next;
                }
        }
        printf(
                "---------------------------------------------------------------------------\n");
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

void launchJob(char *command[], char *file, int newDescriptor,
               int executionMode)
{
        pid_t pid;
        pid = fork();
        switch (pid) {
        case -1:
                perror("MSH");
                exit(EXIT_FAILURE);
                break;
        case 0:
                signal(SIGINT, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGCHLD, &signalHandler_child);
                signal(SIGTTIN, SIG_DFL);
                usleep(20000);
                setpgrp();
                if (executionMode == FOREGROUND)
                        tcsetpgrp(MSH_TERMINAL, getpid());
                if (executionMode == BACKGROUND)
                        printf("[%d] %d\n", ++numActiveJobs, (int) getpid());

                executeCommand(command, file, newDescriptor, executionMode);

                exit(EXIT_SUCCESS);
                break;
        default:
                setpgid(pid, pid);

                jobsList = insertJob(pid, pid, *(command), file, (int) executionMode);

                t_job* job = getJob(pid, BY_PROCESS_ID);

                if (executionMode == FOREGROUND) {
                        putJobForeground(job, FALSE);
                }
                if (executionMode == BACKGROUND)
                        putJobBackground(job, FALSE);
                break;
        }
}

void putJobForeground(t_job* job, int continueJob)
{
        job->status = FOREGROUND;
        tcsetpgrp(MSH_TERMINAL, job->pgid);
        if (continueJob) {
                if (kill(-job->pgid, SIGCONT) < 0)
                        perror("kill (SIGCONT)");
        }

        waitJob(job);
        tcsetpgrp(MSH_TERMINAL, MSH_PGID);
}

void putJobBackground(t_job* job, int continueJob)
{
        if (job == NULL)
                return;

        if (continueJob && job->status != WAITING_INPUT)
                job->status = WAITING_INPUT;
        if (continueJob)
                if (kill(-job->pgid, SIGCONT) < 0)
                        perror("kill (SIGCONT)");

        tcsetpgrp(MSH_TERMINAL, MSH_PGID);
}

void waitJob(t_job* job)
{
        int terminationStatus;
        while (waitpid(job->pid, &terminationStatus, WNOHANG) == 0) {
                if (job->status == SUSPENDED)
                        return;
        }
        jobsList = delJob(job);
}

void killJob(int jobId)
{
        t_job *job = getJob(jobId, BY_JOB_ID);
        kill(job->pid, SIGKILL);
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

void signalHandler_child(int p)
{
        pid_t pid;
        int terminationStatus;
        pid = waitpid(WAIT_ANY, &terminationStatus, WUNTRACED | WNOHANG);
        if (pid > 0) {
                t_job* job = getJob(pid, BY_PROCESS_ID);
                if (job == NULL)
                        return;
                if (WIFEXITED(terminationStatus)) {
                        if (job->status == BACKGROUND) {
                                printf("\n[%d]+  Done\t   %s\n", job->id, job->name);
                                jobsList = delJob(job);
                        }
                } else if (WIFSIGNALED(terminationStatus)) {
                        printf("\n[%d]+  KILLED\t   %s\n", job->id, job->name);
                        jobsList = delJob(job);
                } else if (WIFSTOPPED(terminationStatus)) {
                        if (job->status == BACKGROUND) {
                                tcsetpgrp(MSH_TERMINAL, MSH_PGID);
                                changeJobStatus(pid, WAITING_INPUT);
                                printf("\n[%d]+   suspended [wants input]\t   %s\n",
                                       numActiveJobs, job->name);
                        } else {
                                tcsetpgrp(MSH_TERMINAL, job->pgid);
                                changeJobStatus(pid, SUSPENDED);
                                printf("\n[%d]+   stopped\t   %s\n", numActiveJobs, job->name);
                        }
                        return;
                } else {
                        if (job->status == BACKGROUND) {
                                jobsList = delJob(job);
                        }
                }
                tcsetpgrp(MSH_TERMINAL, MSH_PGID);
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
