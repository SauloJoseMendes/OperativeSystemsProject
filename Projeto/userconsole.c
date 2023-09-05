
#include "header.h"

//Saulo Jose Mendes 2021235944
// Rui Pedro Raposo Moniz 2019225770
void * receiveMessages(void * args);
int sendPipeMessage(char * userConsoleID, char * command);
void exitHandlerUserConsole(){
    closeSemaphores();
    close(consolePipeFD);
    printf("CLOSING USER CONSOLE\n");
    exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT,exitHandlerUserConsole);
    initializeSemaphores();
    mqID=initializeMessageQueue();
    if(argc != 2){
        perror("Número errado de argumentos ./console {identificador da consola}");
        exitHandlerUserConsole();
    }
    if (!isAnInt(argv[1])||atoi(argv[1])<=0){
        perror("O identificador da consola deve ser um numero inteiro maior que zero");
        exitHandlerUserConsole();
    }

    //abre o pipe para a escrita
    if ((consolePipeFD = open(C2SM_PIPE, O_WRONLY)) < 0) {
        unlink(C2SM_PIPE);
        perror("User Console Process is unable to access C2SM pipe");
        exitHandlerUserConsole();
        }
    pthread_t rcvMessagesThread;
    char buffer[COMMANDS_BUFFER_SIZE], bufferCopy[COMMANDS_BUFFER_SIZE], *token, message[100]="USER CONSOLE NUMBER CREATED. ID: ";
    strcat(message,argv[1]);
    writeLog(message);
    int myUserConsoleID= atoi(argv[1]);
    pthread_create(&rcvMessagesThread,NULL,receiveMessages,(void *) &myUserConsoleID);
    do{
        fgets(buffer, COMMANDS_BUFFER_SIZE, stdin);
        if (strlen(buffer)>0){
            buffer[strcspn(buffer, "\n")] = 0;
            strcpy(bufferCopy,buffer);
            token = strtok(bufferCopy, " ");
            if(!strcmp(token,"stats")){
                if (sendPipeMessage(argv[1], buffer)){
                    printf("Key\tLast\tMin\tMax\tAvg\tCount\n");
                }
            }
            else if (!strcmp(token,"reset")){
                sendPipeMessage(argv[1], buffer);
            }
            else if(!strcmp(token,"sensors")) {
                printf("ID\n");
                sendPipeMessage(argv[1], buffer);
            }
            else if (!strcmp(token,"add_alert")){
                sendPipeMessage(argv[1], buffer);
            }
            else if(!strcmp(token,"remove_alert")){
                sendPipeMessage(argv[1], buffer);
            }
            else if(!strcmp(token,"list_alerts")){
                if (sendPipeMessage(argv[1], buffer)){
                    printf("ID\tKey\tMIN\tMAX\n");
                }

            }
            else{
                printf("Unknown command\n");
            }
        }
    }
    while (strcmp(buffer,"exit"));

    exitHandlerUserConsole();
    return 0;
}

void * receiveMessages(void * args){
    myMessage systemManagerMessage;
    int * userConsoleID= (int *) (args);

    while(1) {
        msgrcv(mqID,&systemManagerMessage,MESSAGE_BUFFER_SIZE,*userConsoleID,0);
        printf("%s\n",systemManagerMessage.buffer);

    }
    return NULL;
}

int sendPipeMessage(char * userConsoleID, char * command){
    char message[COMMANDS_BUFFER_SIZE];
    strcpy(message,command);
    strcat(message," *");
    strcat(message,userConsoleID);
    sem_wait(consolePipeSem);
    if (write(consolePipeFD, message, strlen(message)) == -1){
        sem_post(consolePipeSem);
        sleep(2);
        exitHandlerUserConsole();
        return 0;
    } else{
        sem_post(consolePipeSem);
    }
    return 1;
}

