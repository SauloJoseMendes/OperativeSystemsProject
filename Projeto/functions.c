#include "header.h"

//Saulo Jose Mendes 2021235944
// Rui Pedro Raposo Moniz 2019225770

int initializeMessageQueue(){
    int key, msqid;
    if ((key = ftok("./config.txt", 'B')) == -1) {
        perror("ftok");
        exit(1);
    }
    if ((msqid = msgget(key, 0777 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
    }
    return msqid;
}
void closeSemaphores(){
    sem_close(logSem);
    sem_close(dataSem);
    sem_close(workersSem);
    sem_close(sensorsSem);
    sem_close(alertsSem);
    sem_close(consolePipeSem);
    sem_close(sensorPipeSem);
    sem_close(mqSem);
    sem_close(awSem);
    sem_close(requestSem);
    sem_close(workerCounterSem);
    sem_close(iqCounterSem);
}

void unlinkSemNames(){
    if (sem_unlink(LOGSEM) == -1){
        perror("Failed to unlink log semaphore");
        exit(-1);
    }
    if (sem_unlink(DATASEM) == -1){
        perror("Failed to unlink data sempahore");
        exit(-1);
    }

    if (sem_unlink(WORKERSEM) == -1){
        perror("Failed to unlink worker semaphore");
        exit(-1);
    }
    if (sem_unlink(SENSORSEM) == -1){
        perror("Failed to unlink sensors semaphore");
        exit(-1);
    }
    if (sem_unlink(ALERTSEM) == -1){
        perror("Failed to unlink alert semaphore");
        exit(-1);
    }
    if (sem_unlink(CONSOLEPIPESEM) == -1){
        perror("Failed to unlink console pipe semaphore");
        exit(-1);
    }
    if (sem_unlink(SENSORPIPESEM) == -1){
        perror("Failed to unlink sensor pipe semaphore");
        exit(-1);
    }
    if (sem_unlink(MQSEM) == -1){
        perror("Failed to unlink message queue semaphore");
        exit(-1);
    }
    if (sem_unlink(AWSEM) == -1){
        perror("Failed to unlink alerts watcher semaphore");
        exit(-1);
    }
    if (sem_unlink(REQUESTSEM) == -1){
        perror("Failed to unlink requests semaphore");
        exit(-1);
    }
    if (sem_unlink(IQCOUNTERSEM) == -1){
        perror("Failed to unlink internal queue counter semaphore");
        exit(-1);
    }
    if (sem_unlink(WORKERCOUNTERSEM) == -1){
        perror("Failed to unlink worker counter semaphore");
        exit(-1);
    }

}

void unlinkNames(){
    unlinkSemNames();
    if (unlink(C2SM_PIPE) == -1){
        perror("Failed to unlink console to system manager pipe");
        exit(-1);
    }
    if (unlink(S2SM_PIPE) == -1){
        perror("Failed to unlink sensors to system manager");
        exit(-1);
    }
}

void initializeSemaphores(){
    if((logSem = sem_open(LOGSEM, O_CREAT, 0644, 1)) == SEM_FAILED){
        sem_unlink(LOGSEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    if((dataSem = sem_open(DATASEM, O_CREAT, 0644, 1)) == SEM_FAILED){
        sem_unlink(DATASEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    if((sensorsSem = sem_open(SENSORSEM, O_CREAT, 0644, 1)) == SEM_FAILED){
        sem_unlink(SENSORSEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    if((workersSem = sem_open(WORKERSEM, O_CREAT, 0644, 1)) == SEM_FAILED){
        sem_unlink(WORKERSEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    if((alertsSem = sem_open(ALERTSEM, O_CREAT, 0644, 1)) == SEM_FAILED){
        sem_unlink(ALERTSEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    if((consolePipeSem = sem_open(CONSOLEPIPESEM, O_CREAT, 0644, 1)) == SEM_FAILED){
        sem_unlink(CONSOLEPIPESEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    if((sensorPipeSem = sem_open(SENSORPIPESEM, O_CREAT, 0644, 1)) == SEM_FAILED){
        sem_unlink(SENSORPIPESEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    if((mqSem = sem_open(MQSEM, O_CREAT, 0644, 1)) == SEM_FAILED){
        sem_unlink(MQSEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    if((awSem = sem_open(AWSEM, O_CREAT,0700, 0)) == SEM_FAILED){
        sem_unlink(AWSEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    if((requestSem = sem_open(REQUESTSEM, O_CREAT,0700, 0)) == SEM_FAILED){
        sem_unlink(REQUESTSEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }

}

Data *  readConfig(char * filename){
    Data* configData = (Data *) malloc(sizeof(Data));
    FILE *configFile;

    configFile = fopen(filename, "r");

    if (configFile == NULL){
        error("Ficheiro de Configuração inexistente");
    }
    configData->QUEUE_SZ= verifyConfigFileInput(configFile);
    if (configData->QUEUE_SZ < 1){
        error("O número de slots na fila INTERNAL_QUEUE deve ser maior ou igual que 1");
    }

    configData->N_WORKERS=verifyConfigFileInput(configFile);
    if (configData->N_WORKERS < 1){
        error("O número de processos Worker deve ser maior ou igual que 1");
    }
    configData->MAX_KEYS=verifyConfigFileInput(configFile);
    if (configData->MAX_KEYS < 1){
        error("O número máximo de chaves que podem ser armazenadas na memória partilhada deve ser maior ou igual que 1");
    }
    configData->MAX_SENSORS=verifyConfigFileInput(configFile);
    if (configData->MAX_SENSORS < 1){
        error("O número máximo de sensores diferentes que podem ser usados deve ser maior ou igual que 1");
    }
    configData->MAX_ALERTS=verifyConfigFileInput(configFile);
    if (configData->MAX_ALERTS < 0){
        error("O número máximo de alertas que podem estar registados deve ser maior ou igual que 0");
    }
    configData->N_SENSORS=0;
    configData->N_ALERTS=0;
    configData->N_KEYS=0;
    return configData;
}


void writeLog( char *msg) {
    FILE *logFile = fopen("log.txt", "a");
    if (logFile != NULL) {
        sem_wait(logSem);
        //get time info
        time(&timeVar);
        timeinfo = localtime ( &timeVar );
        //print
        fprintf(logFile, "%02d:%02d:%02d %s\n", timeinfo->tm_hour, timeinfo->tm_min,timeinfo->tm_sec, msg);
        printf("%02d:%02d:%02d %s\n", timeinfo->tm_hour, timeinfo->tm_min,timeinfo->tm_sec, msg);
        fflush(logFile);
        fflush(stdout);
        //unlock semaphore
        sem_post(logSem);
    }
    fclose(logFile);
}

void error( char *msg) {
    writeLog(msg);
    perror(msg);
    kill(getpid(),SIGINT);
}

void createSharedMemories(char * filename){
    Data * configData = NULL;
    configData=readConfig(filename);
        if ((dataID = shmget(IPC_PRIVATE, sizeof(Data) , IPC_CREAT | 0600)) < 0) {
            perror(strerror(errno));
            error("Error: Couldn't get/create the shared memory segment!");
        }
        if ((workersID = shmget(IPC_PRIVATE, configData->N_WORKERS * sizeof(Worker), IPC_CREAT | 0600)) < 0) {
            error("Error: Couldn't get/create the shared memory segment!");
        }
        if ((sensorsID = shmget(IPC_PRIVATE,  configData->MAX_KEYS * sizeof(Sensor), IPC_CREAT | 0600)) < 0) {
            error("Error: Couldn't get/create the shared memory segment!");
        }
        if ((alertsID = shmget(IPC_PRIVATE, configData->MAX_ALERTS * sizeof(Alert), IPC_CREAT | 0600)) < 0) {
            error("Error: Couldn't get/create the shared memory segment!");
        }

}

Data * attachDataSharedMemory() {
    Data *data=NULL;
    if ((data = (struct Data *) shmat(dataID, NULL, 0)) == (struct Data *) -1) {
        error("Attachment error in shared memory");

    }
    return data;
}


Worker * attachWorkersSharedMemory(){
    Worker * workers=NULL;
    if ((workers = (Worker *) shmat(workersID, NULL, 0)) == (Worker *) -1){
        error("Attachment error in shared memory");
    }
    return workers;
}
Sensor * attachSensorsSharedMemory(){
    Sensor * sensors=NULL;
    if ((sensors = (struct Sensor *) shmat(sensorsID, NULL, 0)) == (struct Sensor *) -1){
        error("Attachment error in shared memory");
    }
    return sensors;

}
Alert * attachAlertsSharedMemory(){
    Alert * alerts=NULL;
    if ((alerts = (struct Alert *) shmat(alertsID, NULL, 0)) == (struct Alert *) -1){
        error("Attachment error in shared memory");
    }

    return alerts;
}

int verifyConfigFileInput(FILE * configFile){
    char *line = NULL;
    size_t len = 0;
    if(getline(&line,&len,configFile)<=0){
        error("Error reading from config file/Incorrect format");
    }
    line[strcspn(line, "\r\n")] = '\0';
    if (isAnInt(line))return atoi(line);
    else {error("Invalid input in config file");return 0;}
}


int isAnInt(char *s){
    if (s==NULL) return 0;
    int i=0;
    while (s[i]!='\0'){
        if (!isdigit(s[i++])){
            return 0;
        }
    }
    return 1;
}

int isValidSensorID(char *s){
    if (s==NULL) return 0;
    if(strlen(s)>32||strlen(s)<3) return 0;
    int i=0;
    while (s[i]!='\0') {
        if (!isalnum(s[i])){
            return 0;
        }
        i++;
    }
    return 1;
}

int isValidSensorKey(char * s){
    if (s==NULL) return 0;
    if(strlen(s)>32||strlen(s)<3){
        return 0;
    }
    int i=0;
    while (s[i]!='\0') {
        if (!isalnum(s[i])&&s[i]!='_'){
            return 0;
        }
        i++;
    }
    return 1;
}


