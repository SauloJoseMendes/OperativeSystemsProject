#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>


//Saulo Jose Mendes 2021235944
// Rui Pedro Raposo Moniz 2019225770

//MACROS
#define LOGSEM "/logsem"
#define DATASEM "/datasem"
#define WORKERSEM "/workersem"
#define SENSORSEM "/sensorsem"
#define ALERTSEM "/alertsem"
#define CONSOLEPIPESEM "/consolepipesem"
#define SENSORPIPESEM "/sensorpipesem"
#define MQSEM "/messagequeuesem"
#define AWSEM "/alertswatchersem"
#define REQUESTSEM "/internalqueuerequestsem"
#define WORKERCOUNTERSEM "/availableworkersem"
#define IQCOUNTERSEM "/internalqueuecountersem"
#define C2SM_PIPE "/tmp/console_2_system_manager_pipe"
#define S2SM_PIPE "/tmp/sensor_2_system_manager_pipe"





#define MAX(x, y) (((x) > (y)) ? (x) : (y))

//BUFFERSSIZE
#define MESSAGE_BUFFER_SIZE 512
//32+32+2+11+1 (2* max size of ID and Key + 2*# + the max size of an int in string + null terminator) 78
#define SENSOR_BUFFER_SIZE 150
//size of the longest command (add_alert) 100
#define COMMANDS_BUFFER_SIZE 400


typedef struct Data {
    int QUEUE_SZ;
    int N_WORKERS;
    int MAX_KEYS;
    int MAX_SENSORS;
    int MAX_ALERTS;
    int N_SENSORS;
    int N_ALERTS;
    int N_KEYS;
} Data;

typedef struct Sensor {
    char id[32];
    char key[32];
    int lastReading;
    int minReading;
    int maxReading;
    double readingAverage;
    int readingCount;
}Sensor;


typedef struct Alert{
    char id[32];
    char key[32];
    int min;
    int max;
    int consoleID;
    int lastReading;
}Alert;

typedef struct SensorRequest{
    //Type 1 means update value in pre-existing sensor/key pair.
    //Type 2 means create new key for pre-existing sensor and set values
    //Type 3 means create a new sensor/key pair and set values
    int requestType;
    int reading;
    char id[32];
    char key[32];
}SensorRequest;

typedef struct SensorQueueNode{
    SensorRequest sensorRequest;
    struct SensorQueueNode * next;
}SensorQueueNode;

typedef struct SensorQueue{
    SensorQueueNode * begin;
    SensorQueueNode * end;
}SensorQueue;

typedef struct CommandQueueNode{
    char command[COMMANDS_BUFFER_SIZE];
    struct CommandQueueNode * next;
}CommandQueueNode;

typedef struct CommandQueue{
    CommandQueueNode * begin;
    CommandQueueNode * end;
}CommandQueue;


typedef struct myMessage{
    long int messageType;
    char buffer[MESSAGE_BUFFER_SIZE];
}myMessage;

typedef struct Worker{
    int sensorRequestFD[2];
    int commandRequestFD[2];
    int myFDCopy[2];
    pid_t processID;
    int available;
}Worker;




// GLOBAL VARIABLES
int dataID, workersID, sensorsID, alertsID, mqID, consolePipeFD, sensorPipeFD;
pthread_t threadDispatcher, threadConsoleReader, threadSensorReader;
time_t timeVar;


// SEMPAHORES AND MUTEXES
pthread_mutex_t sensorQueueMutex, commandsQueueMutex, sizeMutex;
sem_t * logSem,*dataSem,*sensorsSem,*workersSem,*alertsSem,*consolePipeSem,*sensorPipeSem,*mqSem, * awSem,
*requestSem, *workerCounterSem, *iqCounterSem;
struct tm * timeinfo;




//FUNCTIONS
void error( char *msg);
void writeLog( char *msg);
int verifyConfigFileInput(FILE * configFile);
void unlinkNames();
void createSharedMemories(char * filename);
Data * attachDataSharedMemory();
Worker * attachWorkersSharedMemory();
Sensor * attachSensorsSharedMemory();
Alert * attachAlertsSharedMemory();

void initializeSemaphores();
void closeSemaphores();
void unlinkSemNames();
int initializeMessageQueue();

Data *  readConfig(char * filename);


int isValidSensorID(char *s);
int isValidSensorKey(char *s);
int isAnInt(char *s);