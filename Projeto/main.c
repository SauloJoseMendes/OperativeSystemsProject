#include "header.h"

//Saulo Jose Mendes 2021235944
// Rui Pedro Raposo Moniz 2019225770
_Noreturn void *dispatcherThread(void *args);
void * consoleReaderThread(void *args);
void initializeMutexes();
_Noreturn void * sensorReaderThread(void *args);
void createAlertsWatcher();
void createWorkers();
void exitHandler();
void emptySensorsSharedMemory();
void emptyAlertsSharedMemory();
int availableWorker();
void emptyAlert(Alert * alert);
void emptySensor(Sensor * sensor);
void unlinkNames();
void initializeWorkerCounterSem();
void initializeIQCounterSem();

//Funcoes lista de sensores
int isSensorsAmountAtMaximum();
int isKeyInSharedMemory(char * key, char * id);
SensorQueueNode * createSensorQueueNode(char * string,SensorQueueNode * newSensorQueueNode);
void createSensorInternalQueue();
void insertSensorInternalQueueNode(SensorQueueNode * sensorNode);
int isSensorInSharedMemory(char * id);
int isSensorQueueEmpty();
int isKeysAmountAtMaximum();
SensorQueueNode * getFirstSensorQueueNode();
void cleanSensorInternalQueue();

//Sensors SHM functions
int findAvailableSensorSlotSHM();

//Funcoes lista de commandos
void createCommandInternalQueue();
CommandQueueNode * getFirstCommandQueueNode();
int isCommandQueueEmpty();
void insertCommandQueueNode(char * string, CommandQueueNode * commandQueueNode);
void cleanCommandInternalQueue();

//Funcoes workers
void worker(int index);


Data * configData, * data;
Sensor * sensors;
Worker *workers ;
Alert * alerts ;

SensorQueue * sensorQueue;
CommandQueue * commandQueue;

int myWorkerNumber,sensorInternalQueueSize=0,commandInternalQueueSize=0;
pid_t alertsWatcherID;
int main(int argc, char* argv[]) {
    if (argc!=2){
        printf("home_iot {ficheiro de configuração}\n");
        exit(-1);
    }
    //INICIALIZAR SEMAFOROS
    initializeSemaphores();
    //INICIALIZAR MUTEXES
    initializeMutexes();
    //Open LOG
    writeLog("HOME_IOT SIMULATOR STARTING");
    //CRIAR SHARED MEMORIES
    createSharedMemories(argv[1]);
    //ATTACH SHARED MEMORIES
    data=attachDataSharedMemory();
    workers=attachWorkersSharedMemory();
    sensors=attachSensorsSharedMemory();
    alerts=attachAlertsSharedMemory();
    //Initialize Message Queue
    mqID= initializeMessageQueue();
    //INICIALIZAR SENSOR INTERNAL QUEUE
    createSensorInternalQueue();
    //INICIALIZAR COMMAND INTERNAL QUEUE
    createCommandInternalQueue();
    //INSERIR VALORES DO CONFIG FILE NA MEMORIA PARTILHADA
    configData = readConfig(argv[1]);
    sem_wait(dataSem);
    memcpy(data,configData, sizeof(Data));
    sem_post(dataSem);
    free(configData);
    //INITIALIZE WORKER COUNTER SEM
    initializeWorkerCounterSem();
    //INITIALIZE IQ COUNTER SEM
    initializeIQCounterSem();
    //EMPTY SHARED MEMORY
    emptyAlertsSharedMemory();
    emptySensorsSharedMemory();
    // PROCESSOS WORKER
    createWorkers();
    //INICIAR THREAD DISPATCHER
    pthread_create(&threadDispatcher, NULL, dispatcherThread, NULL);
    //INICIAR THREAD CONSOLE READER
    pthread_create(&threadConsoleReader, NULL, consoleReaderThread, NULL);
    //INICIAR THREAD SENSOR READER
    pthread_create(&threadSensorReader, NULL, sensorReaderThread, NULL);
    // CREATE ALERT WATCHER
    createAlertsWatcher();
    //Definir o sigint
    signal(SIGINT, exitHandler);
    pthread_join(threadConsoleReader,NULL);
}


_Noreturn void *dispatcherThread(void *args) {
    SensorQueueNode * sensorQueueNode;
    CommandQueueNode * commandQueueNode;
    int workerIndex, commandsProcessed;
    while (1){
        sem_wait(requestSem);
        commandsProcessed=0;
        while (!isCommandQueueEmpty()){
            workerIndex=availableWorker();
            commandQueueNode= getFirstCommandQueueNode();
            if (commandQueueNode!=NULL){
                sem_wait(workersSem);
                write(workers[workerIndex].commandRequestFD[1], commandQueueNode->command, sizeof(commandQueueNode->command));
                sem_post(workersSem);
                free(commandQueueNode);
            }
            commandsProcessed++;
            if (commandsProcessed>1)
                sem_wait(requestSem);
            sem_post(iqCounterSem);
        }
        if (!isSensorQueueEmpty()){

            workerIndex=availableWorker();
            sensorQueueNode=getFirstSensorQueueNode();
            if (sensorQueueNode!=NULL){
                sem_wait(workersSem);
                write(workers[workerIndex].sensorRequestFD[1], &sensorQueueNode->sensorRequest, sizeof(SensorRequest));
                sem_post(workersSem);
                free(sensorQueueNode);
            }
            sem_post(iqCounterSem);

        }
    }
}

_Noreturn void * consoleReaderThread(void *args) {
    CommandQueueNode * commandQueueNode;
    size_t n_read;
    writeLog("THREAD CONSOLE_READER CREATED");
    char string[COMMANDS_BUFFER_SIZE];
    //criar pipe c2sm
    if ((mkfifo(C2SM_PIPE, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) {
        unlink(C2SM_PIPE);
        error("System Manager Process is unable to create C2SM pipe");
    }
    //abre o pipe para a escrita
    if ((consolePipeFD = open(C2SM_PIPE, O_RDONLY)) < 0) {
        unlink (C2SM_PIPE);
        error("System Manager Process is unable to access C2SM pipe");
    }
    while(1){
        if ((n_read=read(consolePipeFD, &string, COMMANDS_BUFFER_SIZE)) > 0){
            string[n_read]='\0';
            commandQueueNode = (CommandQueueNode *) malloc(sizeof(CommandQueueNode));
            sem_wait(iqCounterSem);
            insertCommandQueueNode(string,commandQueueNode);

        }
    }
}

_Noreturn void * sensorReaderThread(void *args) {
    SensorQueueNode *newRequest;
    char string[SENSOR_BUFFER_SIZE];
    writeLog("THREAD CONSOLE_READER CREATED");
    //criar pipe c2sm
    if ((mkfifo(S2SM_PIPE, O_CREAT|O_EXCL|0600) < 0) && (errno != EEXIST)) {
        error("System Manager Process is unable to create S2SM pipe");
    }
    //abre o pipe para a escrita
    if ((sensorPipeFD = open(S2SM_PIPE, O_RDONLY)) < 0) {
        unlink(S2SM_PIPE);
        error("System Manager Process is unable to access S2SM pipe");
    }
    while (1) {
        //Espera receber uma mensagem
        if (read(sensorPipeFD, &string, SENSOR_BUFFER_SIZE) == SENSOR_BUFFER_SIZE) {
            //Verificar se a Internal Queue está cheia
            if (sem_trywait(iqCounterSem) != -1) {
                newRequest = (SensorQueueNode *) malloc(sizeof(SensorQueueNode));
                createSensorQueueNode(string, newRequest);
                //verificar se o sensor já está na shared memory
                if (isSensorInSharedMemory(newRequest->sensorRequest.id) >= 0) {
                    //verificar se o sensor já possui esta key
                    if (isKeyInSharedMemory(newRequest->sensorRequest.key,newRequest->sensorRequest.id) >= 0) {

                        // Key e Sensor já estão na shared memory, request type tipo 1 para somente atualizar os valores
                        newRequest->sensorRequest.requestType = 1;
                        insertSensorInternalQueueNode(newRequest);
                    }
                    else{
                        //verificar se já se atingiu o limite máximo de chaves
                        if (!isKeysAmountAtMaximum()) {
                            //Sensor já existe, mas a key é nova, request type tipo 2: criar uma key
                            newRequest->sensorRequest.requestType = 2;
                            insertSensorInternalQueueNode(newRequest);
                        } else {
                            free(newRequest);
                            writeLog("Amount of keys at maximum. Discarding current request");
                        }
                    }
                }
                //no caso de ser um sensor novo
                else {
                    //verificar se já se atingiu o limite máximo de sensores
                    if (!isSensorsAmountAtMaximum()) {
                        //Sensor novo (logo, também key nova), request type 3 para criar ambos
                        newRequest->sensorRequest.requestType = 3;
                        insertSensorInternalQueueNode(newRequest);

                    }
                    else {
                        free(newRequest);
                        writeLog("Amount of sensor at maximum. Discarding current request");
                    }
                }
            } else {
                writeLog("Internal Queue is full. Discarding current request");
            }
        }
    }
}

_Noreturn void alertsWatcher() {
    myMessage message;
    int maxKeys,maxAlerts, numAlerts;
    writeLog("PROCESS ALERTS WATCHER CREATED");
    sem_wait(dataSem);
    maxKeys = data->MAX_KEYS;
    maxAlerts = data->MAX_ALERTS;
    sem_post(dataSem);
    while (1) {
        sem_wait(awSem);
        sem_wait(dataSem);
        numAlerts=data->N_SENSORS;
        sem_post(dataSem);
        if (numAlerts>0){
            sem_wait(alertsSem);
            sem_wait(sensorsSem);
            for (int i = 0; i < maxKeys; ++i) {
                if (strcmp(sensors[i].id, "none") != 0) {
                    for (int j = 0; j < maxAlerts; ++j) {
                        if (strcmp(alerts[j].key, sensors[i].key) == 0) {
                            if (alerts[j].lastReading != sensors[i].lastReading){
                                alerts[j].lastReading = sensors[i].lastReading;
                                if (sensors[i].lastReading < alerts[j].min || sensors[i].lastReading > alerts[j].max) {
                                    snprintf(message.buffer, MESSAGE_BUFFER_SIZE, "WARNING: ALERT TRIGGERED BY:"
                                                                                  "\tSENSOR_ID: %s |"
                                                                                  "SENSOR_KEY: %s |"
                                                                                  "LAST READING: %d", sensors[i].id,
                                             sensors[i].key, sensors[i].lastReading);
                                    message.messageType = alerts[j].consoleID;
                                    sem_wait(mqSem);
                                    msgsnd(mqID, &message, sizeof(message) - sizeof(long), 0);
                                    sem_post(mqSem);
                                    writeLog(message.buffer);
                                }
                            }
                        }
                    }
                }
            }
            sem_post(alertsSem);
            sem_post(sensorsSem);
        }
    }
}

void createAlertsWatcher() {
    if ((alertsWatcherID = fork()) == 0) {
        alertsWatcher();
        }
    }

void createWorkers(){
    char string[100];
    sem_wait(dataSem);
    int maxWorkers= data->N_WORKERS;
    sem_post(dataSem);
    pid_t process_id;
    for (int i = 0; i < maxWorkers; ++i) {
        pipe(workers[i].commandRequestFD);
        pipe(workers[i].sensorRequestFD);
        if ((process_id=fork()) != 0){
            sem_wait(workersSem);
            close(workers[i].commandRequestFD[0]);
            close(workers[i].sensorRequestFD[0]);
            workers[i].processID=process_id;
            workers[i].available=1;
            sem_post(workersSem);
            snprintf(string,100,"WORKER NUMBER %d READY",i);
            writeLog(string);
        }
        else{
            worker(i);
        }
    }
}

void initializeMutexes(){
    if (pthread_mutex_init(&sensorQueueMutex, NULL) != 0) {
        perror("SensorQueueMutex initialization has failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&commandsQueueMutex, NULL) != 0) {
        perror("CommandsQueueMutex initialization has failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&sizeMutex, NULL) != 0) {
        perror("CommandsQueueMutex initialization has failed\n");
        exit(1);
    }
}

void emptySensorsSharedMemory(){
    sem_wait(dataSem);
    int maxKeys= data->MAX_KEYS;
    sem_post(dataSem);
    sem_wait(sensorsSem);
    for (int i = 0; i < maxKeys; ++i) {
        emptySensor(&sensors[i]);
    }
    sem_post(sensorsSem);

}

void emptyAlertsSharedMemory(){
    int maxAlerts;
    sem_wait(dataSem);
    maxAlerts=data->MAX_ALERTS;
    sem_post(dataSem);
    sem_wait(alertsSem);
    for (int i = 0; i < maxAlerts; ++i) {
        emptyAlert(&alerts[i]);
    }
    sem_post(alertsSem);
}
int isSensorsAmountAtMaximum(){
    int result;
    sem_wait(dataSem);
    result = data->MAX_SENSORS == data->N_SENSORS;
    sem_post(dataSem);
    return result;
}

void killWorkers(){
    int isAvailable;
    sem_wait(dataSem);
    int maxWorkers= data->N_WORKERS;
    sem_post(dataSem);
    for (int i = 0; i < maxWorkers; ++i) {
        do{
            sem_wait(workersSem);
            isAvailable=workers[i].available;
            sem_post(workersSem);
        }
        while(isAvailable == 0);
        kill(workers[i].processID,SIGTERM);
        close(workers[i].commandRequestFD[1]);
        close(workers[i].sensorRequestFD[1]);

    }

}
void exitHandler() {
    writeLog("HOME_IOT SIMULATOR WAITING FOR LAST TASKS TO FINISH");
    pthread_cancel(threadSensorReader);
    pthread_cancel(threadConsoleReader);
    pthread_cancel(threadDispatcher);
    //wait for all worker process to end and kill them
    killWorkers();
    //kill alerts watcher
    kill(alertsWatcherID,SIGKILL);
    //destroy internal queue
    cleanSensorInternalQueue();
    cleanCommandInternalQueue();
    writeLog("HOME_IOT SIMULATOR CLOSING");
    closeSemaphores();
    //destroy message queue
    msgctl(mqID,IPC_RMID,0);
    //destroy mutexes
    pthread_mutex_destroy(&sensorQueueMutex);
    pthread_mutex_destroy(&commandsQueueMutex);
    pthread_mutex_destroy(&sizeMutex);
    //destroy shared memory
    shmctl(dataID, IPC_RMID, NULL);
    shmctl(workersID, IPC_RMID, NULL);
    shmctl(sensorsID, IPC_RMID, NULL);
    shmctl(alertsID, IPC_RMID, NULL);
    //unlink names
    unlinkNames();
    exit(0);
}
//Sensor internal queue
int isSensorQueueEmpty(){
    pthread_mutex_lock(&sizeMutex);
    int isEmpty=sensorInternalQueueSize == 0;
    pthread_mutex_unlock(&sizeMutex);
    return isEmpty;
}


SensorQueueNode * createSensorQueueNode(char * string,SensorQueueNode * newSensorQueueNode){
    char *token= strtok(string,"#");
    strcpy(newSensorQueueNode->sensorRequest.id,token);
    token= strtok(NULL,"#");
    strcpy(newSensorQueueNode->sensorRequest.key,token);
    token= strtok(NULL,"#");
    newSensorQueueNode->sensorRequest.reading= (int) strtol(token, NULL, 10);
    newSensorQueueNode->next=NULL;
    return newSensorQueueNode;
}
void createSensorInternalQueue(){
    sensorQueue= (SensorQueue *)malloc(sizeof(SensorQueue));
    sensorQueue->begin=NULL;
    sensorQueue->end=NULL;
}

void insertSensorInternalQueueNode(SensorQueueNode * sensorNode){
    pthread_mutex_lock(&sensorQueueMutex);
    if (sensorQueue!=NULL){
        if (sensorNode!=NULL){
            if (sensorQueue->begin==NULL){
                sensorQueue->begin=sensorNode;
                sensorQueue->end=sensorNode;
            } else{
                sensorQueue->end->next=sensorNode;
                sensorQueue->end=sensorNode;
            }
            pthread_mutex_lock(&sizeMutex);
            sensorInternalQueueSize++;
            sem_post(requestSem);
            pthread_mutex_unlock(&sizeMutex);
        }
    }
    pthread_mutex_unlock(&sensorQueueMutex);
}

SensorQueueNode * getFirstSensorQueueNode(){
    if (sensorQueue!=NULL){
        pthread_mutex_lock(&sensorQueueMutex);
        SensorQueueNode * temp= sensorQueue->begin;
        if (sensorQueue->begin==sensorQueue->end){
            sensorQueue->begin=NULL;
            sensorQueue->end=NULL;
        } else{
            sensorQueue->begin=sensorQueue->begin->next;
        }
        pthread_mutex_unlock(&sensorQueueMutex);
        pthread_mutex_lock(&sizeMutex);
        sensorInternalQueueSize--;
        pthread_mutex_unlock(&sizeMutex);
        return temp;
    }
    return NULL;
}

int isSensorInSharedMemory(char * id){
    sem_wait(dataSem);
    int maxKeys= data->MAX_KEYS;
    sem_post(dataSem);
    sem_wait(sensorsSem);
    if (sensors!=NULL){
        for (int i = 0; i < maxKeys; ++i) {
            if(strcmp(sensors[i].id,id) == 0){
                sem_post(sensorsSem);
                return i;

            }
        }
    }
    sem_post(sensorsSem);
    return -1;
}

int isKeysAmountAtMaximum(){
    int result;
    sem_wait(dataSem);
    result = (data->MAX_KEYS == data->N_KEYS);
    sem_post(dataSem);
    return result;
}

int findAvailableSensorSlotSHM(){
    sem_wait(dataSem);
    int maxKeys= data->MAX_KEYS;
    sem_post(dataSem);
    sem_wait(sensorsSem);
    for (int i = 0; i < maxKeys; ++i) {
        if (sensors[i].lastReading==-1){
            sem_post(sensorsSem);
            return i;
        }

    }
    sem_post(sensorsSem);
    return -1;
}

int isKeyInSharedMemory(char * key, char *id) {
    sem_wait(dataSem);
    int maxKeys= data->MAX_KEYS;
    sem_post(dataSem);
    sem_wait(sensorsSem);
    if (sensors != NULL) {
        for (int i = 0; i < maxKeys; ++i) {
            if (strcmp(sensors[i].id, id) == 0 && strcmp(sensors[i].key, key) == 0) {
                sem_post(sensorsSem);
                return i;
            }
        }
    }
    sem_post(sensorsSem);
    return -1;
}

int isCommandQueueEmpty(){
    int result;
    pthread_mutex_lock(&sizeMutex);
    result= commandInternalQueueSize == 0;
    pthread_mutex_unlock(&sizeMutex);
    return result;
}

int availableWorker() {
    sem_wait(workerCounterSem);
    sem_wait(dataSem);
    int maxWorkers = data->N_WORKERS;
    sem_post(dataSem);
    sem_wait(workersSem);
    for (int i = 0; i < maxWorkers; ++i) {
        if (workers[i].available) {
            sem_post(workersSem);
            sem_post(workerCounterSem);
            return i;
        }
    }
    sem_post(workersSem);
    sem_post(workerCounterSem);
    return -1;
}

void createCommandInternalQueue(){
    commandQueue= (CommandQueue *) malloc(sizeof(CommandQueue));
    commandQueue->begin=NULL;
    commandQueue->end=NULL;

}

void insertCommandQueueNode(char * string, CommandQueueNode * commandQueueNode){

    if (commandQueue!=NULL){
        if (commandQueueNode!=NULL){
            pthread_mutex_lock(&commandsQueueMutex);
            strcpy(commandQueueNode->command,string);
            if (commandQueue->begin==NULL){
                commandQueue->begin=commandQueueNode;
                commandQueue->end=commandQueueNode;
            } else{
                commandQueue->end->next=commandQueueNode;
                commandQueue->end=commandQueueNode;
            }
            pthread_mutex_unlock(&commandsQueueMutex);
            pthread_mutex_lock(&sizeMutex);
            commandInternalQueueSize++;
            sem_post(requestSem);
            pthread_mutex_unlock(&sizeMutex);
        }
    }

}

CommandQueueNode * getFirstCommandQueueNode(){
    if (commandQueue!=NULL){
        pthread_mutex_lock(&commandsQueueMutex);
        CommandQueueNode * temp =commandQueue->begin;
        if (commandQueue->begin==commandQueue->end){
            commandQueue->begin=NULL;
            commandQueue->end=NULL;
        } else{
            commandQueue->begin=commandQueue->begin->next;
        }
        pthread_mutex_unlock(&commandsQueueMutex);
        pthread_mutex_lock(&sizeMutex);
        commandInternalQueueSize--;
        pthread_mutex_unlock(&sizeMutex);
        return temp;
    }
    return NULL;
}

void insertSensorInSHM(SensorRequest sensorRequest, int i){
    sem_wait(sensorsSem);
    strcpy(sensors[i].id,sensorRequest.id);
    strcpy(sensors[i].key,sensorRequest.key);
    sensors[i].lastReading=sensorRequest.reading;
    sensors[i].minReading=sensorRequest.reading;
    sensors[i].maxReading=sensorRequest.reading;
    sensors[i].readingAverage= ((double)sensorRequest.reading/ ++(sensors[i].readingCount));
    sem_post(sensorsSem);
}
void keyToString(char * string,Sensor * sensor){
    snprintf(string, MESSAGE_BUFFER_SIZE, "%s\t%d\t%d\t%d\t%f\t%d", sensor->key,sensor->lastReading,sensor->minReading,
             sensor->maxReading,sensor->readingAverage,sensor->readingCount);
}

void alertToString(char* string, Alert * alert){
    snprintf(string, MESSAGE_BUFFER_SIZE, "%s\t%s\t%d\t%d",alert->id,alert->key,alert->min,alert->max);
}

void processSensorRequest(SensorRequest sensorRequest){
    int i;
    if (sensorRequest.requestType==1){
        if ((i= isKeyInSharedMemory(sensorRequest.key, sensorRequest.id)) > -1){
            sem_wait(sensorsSem);
            sensors[i].lastReading=sensorRequest.reading;
            if (sensorRequest.reading>sensors[i].maxReading)
                sensors[i].maxReading=sensorRequest.reading ;
            if(sensorRequest.reading<sensors[i].minReading)
                sensors[i].minReading=sensorRequest.reading;
            sensors[i].readingAverage= ((sensors[i].readingAverage * sensors[i].readingCount) + sensorRequest.reading);
            sensors[i].readingCount++;
            sensors[i].readingAverage/=sensors[i].readingCount;
            sem_post(sensorsSem);
            sem_post(awSem);
        }
    }
    else if (sensorRequest.requestType==2){
        if (!isKeysAmountAtMaximum()){
            if ((i=findAvailableSensorSlotSHM())>-1){
                insertSensorInSHM(sensorRequest,i);
                sem_wait(dataSem);
                data->N_KEYS++;
                sem_post(dataSem);

            }
        }
    }
    else if(sensorRequest.requestType==3){
        if (!isKeysAmountAtMaximum()){
            if (!isSensorsAmountAtMaximum()){
                if ((i=findAvailableSensorSlotSHM())>-1){
                    insertSensorInSHM(sensorRequest,i);
                    sem_wait(dataSem);
                    data->N_KEYS++;
                    data->N_SENSORS++;
                    sem_post(dataSem);
                }
            }
        }
    }
}
void sendStats(int consoleID){
    myMessage message;
    message.messageType=consoleID;
    char string[MESSAGE_BUFFER_SIZE];
    sem_wait(dataSem);
    int maxKeys=data->MAX_KEYS;
    sem_post(dataSem);
    sem_wait(sensorsSem);
    for (int i = 0; i < maxKeys; ++i) {
        if (strcmp(sensors[i].id,"none") != 0){
            keyToString(string,&sensors[i]);
            strcpy(message.buffer,string);
            sem_wait(mqSem);
            msgsnd(mqID, &message, sizeof(message)-sizeof(long), 0);
            sem_post(mqSem);

        }
    }
    sem_post(sensorsSem);
}

void sendAlerts(int consoleID){
    myMessage message;
    message.messageType=consoleID;
    char string[MESSAGE_BUFFER_SIZE];
    sem_wait(dataSem);
    int maxAlerts=data->MAX_ALERTS;
    sem_post(dataSem);
    sem_wait(alertsSem);
    for (int i = 0; i < maxAlerts; ++i) {
        if (strcmp(alerts[i].id,"none")!=0){
            alertToString(string,&alerts[i]);
            strcpy(message.buffer,string);
            sem_wait(mqSem);
            msgsnd(mqID, &message, sizeof(message)-sizeof(long), 0);
            sem_post(mqSem);
        }
    }
    sem_post(alertsSem);
}
void sendSensors(int consoleID){
    myMessage message;
    message.messageType=consoleID;
    sem_wait(dataSem);
    int maxKeys=data->MAX_KEYS;
    sem_post(dataSem);
    sem_wait(sensorsSem);
    for (int i = 0; i < maxKeys; ++i) {
        if (strcmp(sensors[i].id,"none") != 0){
            strcpy(message.buffer,sensors[i].id);
            message.buffer[strlen(sensors[i].id)]='\0';
            sem_wait(mqSem);
            msgsnd(mqID, &message, sizeof(message)-sizeof(long), 0);
            sem_post(mqSem);
        }
    }
    sem_post(sensorsSem);
}


int insertAlertInSHM(char *id, char*key, int min, int max, int userConsoleID){
    sem_wait(dataSem);
    int maxAlerts=data->MAX_SENSORS;
    int numAlerts=data->N_ALERTS;
    sem_post(dataSem);
    if(numAlerts<maxAlerts){
            if (isKeyInSharedMemory(key, id)){
                int i=0;
                sem_wait(alertsSem);
                while(i<maxAlerts&&alerts[i].min!=-1)i++;
                if (alerts[i].min==-1){
                    strcpy(alerts[i].id,id);
                    strcpy(alerts[i].key,key);
                    alerts[i].min=min;
                    alerts[i].max=max;
                    alerts[i].consoleID=userConsoleID;
                    alerts[i].lastReading=99999;
                    //update number of sensors;
                    sem_wait(dataSem);
                    data->N_ALERTS++;
                    sem_post(dataSem);
                }
                sem_post(alertsSem);
                return 1;
            }
            return 0;

    }
    return 0;
}

void removeAlert(int consoleID, char * id){
    sem_wait(dataSem);
    int maxAlerts= data->MAX_ALERTS;
    sem_post(dataSem);
    sem_wait(alertsSem);
    for (int i = 0; i < maxAlerts; ++i) {
        if (strcmp(alerts[i].id, id)==0){
            emptyAlert(&alerts[i]);
            sem_wait(dataSem);
            data->N_ALERTS--;
            sem_post(dataSem);
        }
    }
    sem_post(alertsSem);
}


void sendFeedback(int consoleID, char* s){
    myMessage message;
    strcpy(message.buffer,s);
    message.messageType=consoleID;
    sem_wait(mqSem);
    msgsnd(mqID, &message, sizeof(message)-sizeof(long), 0);
    sem_post(mqSem);
}

void processCommandRequest(char * commandRequest){
    char * command, * stringConsoleID;
    int consoleID;
    stringConsoleID=strchr(commandRequest, '*');
    stringConsoleID++;
    command=strtok(commandRequest, " ");
    if (isAnInt(stringConsoleID)){
        consoleID= (int) strtol(stringConsoleID, NULL, 10);
        if (!strcmp(command, "stats")){
            sendStats(consoleID);
        }
        else if (!strcmp(command, "sensors")){
            sendSensors(consoleID);
        }
        else if(!strcmp(command,"add_alert")){
            char * id = strtok(NULL, " ");
            if (isValidSensorID(id)){
                char * key =strtok(NULL, " ");
                if (isValidSensorKey(key)){
                    char * min= strtok(NULL, " ");
                    if (isAnInt(min)){
                        char * max =strtok(NULL, " ");
                        if (isAnInt(max)){
                            if(insertAlertInSHM(id, key, (int) strtol(min, NULL, 10), (int) strtol(max, NULL, 10),consoleID)){
                                sendFeedback(consoleID, "OK");
                            }
                            else
                                sendFeedback(consoleID, "ERRO");
                        }
                        else
                            sendFeedback(consoleID, "ERRO");
                    }
                    else
                        sendFeedback(consoleID, "ERRO");
                }
                else
                    sendFeedback(consoleID, "ERRO");
            }
            else
                sendFeedback(consoleID, "ERRO");
        }
        else if(!strcmp(command, "reset")){
            emptySensorsSharedMemory();
            sem_wait(dataSem);
            data->N_SENSORS=0;
            data->N_KEYS=0;
            sem_post(dataSem);
            sendFeedback(consoleID,"OK");
        }
        else if(!strcmp(command, "remove_alert")){
            char * id = strtok(NULL, " ");
            if (isValidSensorID(id)) {
                removeAlert(consoleID,id);
                sendFeedback(consoleID, "OK");
            }
            else{
                sendFeedback(consoleID, "ERRO");
            }
        }
        else if(!strcmp(command,"list_alerts")){
            sendAlerts(consoleID);
        }
    }
}
void imBusy(int index){
    sem_wait(workersSem);
    workers[index].available=0;
    sem_post(workersSem);
}
void imFree(int index){
    sem_wait(workersSem);
    workers[index].available=1;
    sem_post(workersSem);
}
void imGonnaDie(){
    close(workers[myWorkerNumber].commandRequestFD[0]);
    close(workers[myWorkerNumber].sensorRequestFD[0]);
    close(workers[myWorkerNumber].myFDCopy[0]);
    close(workers[myWorkerNumber].myFDCopy[1]);
    exit(0);
}
_Noreturn void worker(int index){
    signal(SIGTERM,imGonnaDie);
    myWorkerNumber=index;
    char command[COMMANDS_BUFFER_SIZE];
    close(workers[index].commandRequestFD[1]);
    close(workers[index].sensorRequestFD[1]);
    SensorRequest sensorRequest;
    ssize_t n_bytes;
    imFree(index);

    while(1){
        workers[index].myFDCopy[0]=dup(workers[index].commandRequestFD[0]);
        workers[index].myFDCopy[1]=dup(workers[index].sensorRequestFD[0]);
        fd_set read_set;
        FD_ZERO(&read_set);
        for (int i = 0; i < 2; ++i) {
            FD_SET(workers[index].myFDCopy[i], &read_set);
        }
        if (select(MAX(workers[index].myFDCopy[0], workers[index].myFDCopy[1]) + 1, &read_set, NULL, NULL, NULL) > 0 ) {
            if (FD_ISSET(workers[index].myFDCopy[0], &read_set)) {
                sem_wait(workerCounterSem);
                imBusy(index);
                read(workers[index].commandRequestFD[0], command, COMMANDS_BUFFER_SIZE);
                processCommandRequest(command);
                imFree(index);
                sem_post(workerCounterSem);
            }
            else if(FD_ISSET( workers[index].myFDCopy[1], &read_set)){
                sem_wait(workerCounterSem);
                imBusy(index);
                n_bytes= read(workers[index].sensorRequestFD[0],&sensorRequest, sizeof(SensorRequest));
                if (n_bytes== sizeof(SensorRequest)){
                    processSensorRequest(sensorRequest);
                }
                imFree(index);
                sem_post(workerCounterSem);
            }
            for (int i = 0; i < 2; ++i) {
                close(workers[index].myFDCopy[i]);
            }


        }

    }

}

void emptyAlert(Alert * alert){
    strcpy(alert->id,"none");
    strcpy(alert->key,"none");
    alert->min=-1;
    alert->max=-1;
    alert->consoleID=-1;
}

void emptySensor(Sensor * sensor){
    strcpy(sensor->id,"none");
    strcpy(sensor->key,"none");
    sensor->minReading=-1;
    sensor->maxReading=-1;
    sensor->readingAverage=-1;
    sensor->lastReading=-1;
    sensor->readingCount=0;
}

void cleanSensorInternalQueue(){
    SensorQueueNode * temp = sensorQueue->begin, * toClean;
    char string[500];
    pthread_mutex_lock(&sensorQueueMutex);
    while (temp!=NULL){
        toClean=temp;
        snprintf(string, 500, "Task not executed: SENSOR ID:%s\tKEY:%s\tReading: %d\tRequest Type: %d\t",
                 toClean->sensorRequest.id,toClean->sensorRequest.key,
                 toClean->sensorRequest.reading,toClean->sensorRequest.requestType);
        writeLog(string);
        temp=temp->next;
        free(toClean);
    }
    free(sensorQueue);
    pthread_mutex_unlock(&sensorQueueMutex);
}
void cleanCommandInternalQueue(){
    pthread_mutex_lock(&commandsQueueMutex);
    CommandQueueNode * temp = commandQueue->begin, * toClean;
    char string[COMMANDS_BUFFER_SIZE + 50];
    while (temp!=NULL){
        toClean=temp;
        snprintf(string, COMMANDS_BUFFER_SIZE + 20, "Task not executed: %s",toClean->command);
        writeLog(string);
        temp=temp->next;
        free(toClean);
    }
    free(commandQueue);
    pthread_mutex_unlock(&commandsQueueMutex);
}

void initializeWorkerCounterSem(){
    sem_wait(dataSem);
    if((workerCounterSem = sem_open(WORKERCOUNTERSEM, O_CREAT,0700, data->N_WORKERS)) == SEM_FAILED){
        unlink(WORKERCOUNTERSEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    sem_post(dataSem);
}
void initializeIQCounterSem(){
    sem_wait(dataSem);
    if((iqCounterSem = sem_open(IQCOUNTERSEM, O_CREAT,0700, data->QUEUE_SZ)) == SEM_FAILED){
        unlink(IQCOUNTERSEM);
        perror("Error initializing semaphore:");
        exit(-1);
    }
    sem_post(dataSem);
}