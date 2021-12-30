#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include "csapp.h"
// Number of cells vertically/horizontally in the grid
#define GRIDSIZE 10

sem_t topLock;
int topScore = 0;
sem_t mapLock;
volatile char map[GRIDSIZE][GRIDSIZE];

volatile int scores[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
volatile int numPlayers = 0;

bool shouldExit = false;
int level;
int numTomatoes;

void *thread(void *vargp);

// get a random value in the range [0, 1]
double rand01()
{
    return (double) rand() / (double) RAND_MAX;
} 

void printMap(){
    printf("LVL: %d\n",level);
    printf("TOMATOES: %d\n",numTomatoes);
    printf("TOP SCORE: %d\n",topScore);
    printf("NUM PLAYERS: %d\n",numPlayers);
    for(int i = 0; i < GRIDSIZE; i++){
        for(int j = 0; j < GRIDSIZE; j++){
            printf("%c", map[j][i]);
        }
        printf("\n");
    }
}

void sendMap(int connfd, int *x, int *y, int score){
    char mapBuf[102];
    char dataBuf[48];

    int z = 0;
    for(int i = 0; i < GRIDSIZE; i++){
        for(int j = 0; j < GRIDSIZE; j++){
            if(j == *x && i == *y){
                mapBuf[z] = '@';
            }else{
                mapBuf[z] = map[j][i];
            }
            z++;
        }
    }
    if(score >= topScore){
        mapBuf[z] = '!';
    }else{
        mapBuf[z] = '.';
    }
    z++;
    mapBuf[z] = '\n';
    Rio_writen(connfd, mapBuf, 102);
    //printf("FILE D: %d %s\n",connfd, mapBuf);
    z = 0;
    dataBuf[z] = (char) score;
    z++;
    dataBuf[z] = (char) (score >> 8);
    z++;
    dataBuf[z] = (char) (score >> 16);
    z++;
    dataBuf[z] = (char) (score >> 24);
    z++;
    dataBuf[z] = (char) level;
    z++;
    dataBuf[z] = (char) (level >> 8);
    z++;
    dataBuf[z] = (char) (level >> 16);
    z++;
    dataBuf[z] = (char) (level >> 24);
    z++;
    dataBuf[z] = (char) numTomatoes;
    z++;
    dataBuf[z] = (char) (numTomatoes >> 8);
    z++;
    dataBuf[z] = (char) (numTomatoes >> 16);
    z++;
    dataBuf[z] = (char) (numTomatoes >> 24);
    z++;
    dataBuf[z] = (char) topScore;
    z++;
    dataBuf[z] = (char) (topScore >> 8);
    z++;
    dataBuf[z] = (char) (topScore >> 16);
    z++;
    dataBuf[z] = (char) (topScore >> 24);
    z++;

    for(int i = 0; i < 8; i++){
        dataBuf[z] = (char) scores[i];
        z++;
        dataBuf[z] = (char) (scores[i] >> 8);
        z++;
        dataBuf[z] = (char) (scores[i] >> 16);
        z++;
        dataBuf[z] = (char) (scores[i] >> 24);
        z++;
    }

    Rio_writen(connfd, dataBuf, 48);
}

void initGrid()
{
    level++;
    printf("*lvl%d*\n",level);
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            double r = rand01();
            if (r < 0.1 && map[i][j] != 'P') {
                map[i][j] = 'T';
                numTomatoes++;
            }
            else
                map[i][j] = 'G';
        }
    }

    // ensure grid isn't empty
    while (numTomatoes == 0)
        initGrid();

    printMap(); 
}

int initPlayer(int connfd,int *x,int *y){
    if(numPlayers >= 8){
        return -1;
    }
    *x = rand01() * 9;
    *y = rand01() * 9;

    if(map[*x][*y] == 'P'){
        return initPlayer(connfd, x, y);
    }

    if (map[*x][*y] == 'T') {
        map[*x][*y] = 'P';
        numTomatoes--;
    }else{
        map[*x][*y] = 'P';
    }
    numPlayers++;
    printMap();

    for(int i = 0; i < 8; i++){
        if(scores[i] == -1){
             return i;
        }
    } 
   return -1;
}

//returns score increment
int updatePlayer(int connfd, int *x, int *y, int chngX, int chngY, int score){

    int tempX = *x + chngX;
    int tempY = *y + chngY;

    P(&mapLock);
    if(tempX >= 0 && tempY >= 0 && tempX < GRIDSIZE && tempY < GRIDSIZE && map[tempX][tempY] != 'P'){
        map[*x][*y] = 'G';
        *x = tempX;
        *y = tempY;
        if(map[*x][*y] == 'T'){
            score++;
            numTomatoes--;
        }
        map[*x][*y] = 'P';
         
    }
    sendMap(connfd, x, y,score);
    V(&mapLock);
    if(score > topScore){
        P(&topLock);
        topScore = score;
        V(&topLock);
    }
    return score;
}

int main(int argc, char* argv[])
{
    char* port = argv[1];
    if(argc < 2){
        printf("Usage: server <hosting port>\n");
        return 0;
    }

    printf("Initilizing Server!\n");
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; /* Enough room for any addr */ 
    pthread_t tid;
   
    listenfd = Open_listenfd(port);

    printf("Listening on 1025!\n");
    srand(time(NULL));

    Sem_init(&topLock, 0, 1);
    Sem_init(&mapLock, 0, 1);

    initGrid();
    char client_hostname[MAXLINE], client_port[MAXLINE];

    // main game loop
    while (!shouldExit) {
        clientlen = sizeof(struct sockaddr_storage); /* Important! */
        connfdp = Malloc(sizeof(int)); //line:conc:echoservert:beginmalloc
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen); //line:conc:echoservert:endmalloc
        getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Player connected on (%s, %s)\n", client_hostname, client_port);
        Pthread_create(&tid, NULL, thread, connfdp);
    }

}

/* thread routine */
void *thread(void *vargp) 
{ 
    int score = 0;

    int x = 0;
    int y = 0;

    int sI;

    int connfd = *((int *)vargp);
    
    Pthread_detach(pthread_self()); //line:conc:echoservert:detach
    Free(vargp);                    //line:conc:echoservert:free
    
    P(&mapLock);
    if((sI = initPlayer(connfd, &x, &y)) == -1){
        Rio_writen(connfd, "~8 Players have joined. Server is full!\n\0", strlen("~8 Players have joined. Server is full!\n\0"));
        printf("SERVER FULL\n");
        V(&mapLock);
        Close(connfd);
        pthread_exit(NULL);
    }else{
        printf("New Player FD: %d\n", connfd);
    }
    V(&mapLock);

    size_t n = 0;
    char buf[MAXLINE];
    rio_t rio;
    Rio_writen(connfd, "Connection with server established!\n\0", strlen("Connection with server established!\n\0"));
    Rio_readinitb(&rio, connfd);
    P(&mapLock);
    sendMap(connfd, &x, &y, 0);
    V(&mapLock);
    while(!shouldExit){
                n = Rio_readnb(&rio, buf, 1);
                //Rio_writen(connfd, buf, n);
                if((int)n == 0){
                    printf("Player FD# %d has left\n", connfd);
                    P(&mapLock);
                    map[x][y] = 'G';
                    V(&mapLock);
                    scores[sI] = -1;
                    numPlayers--;
                    printMap();
                    break;
                }
                if(buf[0] != '*'){
                    printf("server received %d bytes %s\n", (int)n, buf);
                }
                if(buf[0] == 'w'){
                    score = updatePlayer(connfd, &x, &y, 0,-1, score);
                    printMap();
                }
                if(buf[0] == 's'){
                    score = updatePlayer(connfd, &x, &y, 0, 1, score);
                    printMap();
                }
                if(buf[0] == 'a'){
                    score = updatePlayer(connfd, &x, &y, -1, 0, score);
                    printMap();
                }
                if(buf[0] == 'd'){
                    score = updatePlayer(connfd, &x, &y, 1, 0, score);
                    printMap();
                }
                if(buf[0] == '*'){
                    score = updatePlayer(connfd, &x, &y, 0, 0, score);
                }
                scores[sI] = score;

                if(numTomatoes == 0){
                    P(&mapLock);
                    initGrid();
                    V(&mapLock);
                    printMap();
                }

            }
            

    Close(connfd);
    return NULL;
}
