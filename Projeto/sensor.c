#include "header.h"

//Saulo Jose Mendes 2021235944
// Rui Pedro Raposo Moniz 2019225770
int n_leituras=0;

void printReadings(){
    printf("Number of readings sent:\t%d\n",n_leituras);
}

void exitHandlerSensor(){
    closeSemaphores();
    close(sensorPipeFD);
    printf("CLOSING SENSOR PROCESS\n");
    exit(0);
}

int getRandomReading(int min, int max) {
    srand(time(NULL));  // seed the random number generator with the current time
    return rand() % (max - min + 1) + min;  // generate a random number between a and b (inclusive)
}

int main(int argc, char* argv[]) {
    initializeSemaphores();
    signal(SIGTSTP,printReadings);
    signal(SIGINT,exitHandlerSensor);
    int sendingGap,min,max;
    int leitura;
    if(argc != 6){
        perror("Número errado de argumentos\nsensor {identificador do sensor} {intervalo entre envios em segundos "
              "(>=0)} {chave} {valor inteiro mínimo a ser enviado} {valor inteiro máximo a ser enviado}\n");
        exitHandlerSensor();
    }

    if (!isValidSensorID(argv[1])){
        perror("Invalid ID");
        exitHandlerSensor();
    }
    if(!isAnInt(argv[2])&&atoi(argv[2])<0){
        perror("O intervalo em segundos entre envios deve ser maior ou igual que 0\n");
        exitHandlerSensor();
    }
    if (!isValidSensorKey(argv[3])){
        perror("Invalid key");
        exitHandlerSensor();
    }
    if(isAnInt(argv[4])&&atoi(argv[4])<0){
        perror("O valor minimo  deve ser maior ou igual que 0\n");
        exitHandlerSensor();
    }
    if(isAnInt(argv[5])&&atoi(argv[5])<=atoi(argv[4])){
        perror("O valor maximo deve ser maior ou igual que o valor minimo\n");
        exitHandlerSensor();
    }
    //criar pipe s2sm
    if ((mkfifo(S2SM_PIPE, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) {
        perror("User Console Process is unable to create C2SM pipe");
        exitHandlerSensor();
    }
    //abre o pipe para a escrita
    if ((sensorPipeFD = open(S2SM_PIPE, O_WRONLY)) < 0) {
        unlink(S2SM_PIPE);
        perror("User Console Process is unable to access C2SM pipe");
        exitHandlerSensor();
    }


    char string[SENSOR_BUFFER_SIZE];
    //Dois caracteres # mais 11 do tamanho maximo de uma representacao de um inteiro em string (a leitura) mais terminal char
    sendingGap=atoi(argv[2]);
    min=atoi(argv[4]);
    max=atoi(argv[5]);

    while (1) {
        leitura = getRandomReading(min, max);
        snprintf(string, SENSOR_BUFFER_SIZE, "%s#%s#%d", argv[1], argv[3], leitura);
        sem_wait(sensorPipeSem);
        if (write(sensorPipeFD, string, SENSOR_BUFFER_SIZE) != -1){
            n_leituras++;
            sem_post(sensorPipeSem);
        }
        else {
            if (errno == EPIPE)
                printf("Broken pipe\n");
            sem_post(sensorPipeSem);
            exitHandlerSensor();
        }
        printf("%s\n", string);



        sleep(sendingGap);
    }

    return 0;
}
