#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include "csapp.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

// Dimensions for the drawn grid (should be GRIDSIZE * texture dimensions)
#define GRID_DRAW_WIDTH 832
#define GRID_DRAW_HEIGHT 640

#define tv2dbl(tv) ((tv).tv_sec + (tv).tv_usec / 1000000.0)

#define WINDOW_WIDTH (GRID_DRAW_WIDTH)
#define WINDOW_HEIGHT (HEADER_HEIGHT + GRID_DRAW_HEIGHT)

// Header displays current score
#define HEADER_HEIGHT 50

// Number of cells vertically/horizontally in the grid
#define GRIDSIZE 10


int hasTop = 0;
int highScore;
int scores[8] = {-1,-1,-1,-1,-1,-1,-1,-1};

char map[GRIDSIZE][GRIDSIZE];

int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

typedef struct
{
    int x;
    int y;
} Position;


typedef enum
{
    TILE_GRASS,
    TILE_TOMATO
} TILETYPE;

TILETYPE grid[GRIDSIZE][GRIDSIZE];

Position playerPosition;
int score = 0;
int level = 0;
int numTomatoes = 0;

bool shouldExit = false;

TTF_Font* font;

// get a random value in the range [0, 1]
double rand01()
{
    return (double) rand() / (double) RAND_MAX;
}

void printMap(){
    printf("\nScore: %d\nTomatoes: %d\nLevel: %d\n", score, numTomatoes, level);
    for(int i = 0; i < GRIDSIZE; i++){
        for(int j = 0; j < GRIDSIZE; j++){
            printf("%c", map[j][i]);
        }
        printf("\n");
    }
}

void readMap(){
    //printf("STARTING READ\n");
    char mapBuf[MAXLINE];
    char dataBuf[MAXLINE];
    Rio_readlineb(&rio, mapBuf, MAXLINE);
    //printf("%s",mapBuf);
    int z = 0;
    for(int i = 0; i < GRIDSIZE; i++){
        for(int j = 0; j < GRIDSIZE; j++){
            map[j][i] = mapBuf[z];
            z++;
        }
    }
    if(mapBuf[z] == '!'){
        hasTop = 1;
    }else{
        hasTop = 0;
    }
    z = 0;
    Rio_readnb(&rio, dataBuf, 48);
    score = ((int)dataBuf[z] & 255) | ((int)(dataBuf[z+1] & 255) << 8) | ((int)(dataBuf[z+2] & 255) << 16) | ((int)(dataBuf[z+3] & 255) << 24);
    z = z + 4;
    level = ((int)dataBuf[z] & 255) | ((int)(dataBuf[z+1] & 255) << 8) | ((int)(dataBuf[z+2] & 255) << 16) | ((int)(dataBuf[z+3] & 255) << 24);
    z = z + 4;
    numTomatoes = ((int)dataBuf[z] & 255) | ((int)(dataBuf[z+1] & 255) << 8) | ((int)(dataBuf[z+2] & 255) << 16) | ((int)(dataBuf[z+3] & 255) << 24);
    z = z + 4;
    highScore = ((int)dataBuf[z] & 255) | ((int)(dataBuf[z+1] & 255) << 8) | ((int)(dataBuf[z+2] & 255) << 16) | ((int)(dataBuf[z+3] & 255) << 24);
    z = z + 4;

    for(int i = 0; i < 8; i++){
        scores[i] = ((int)dataBuf[z] & 255) | ((int)(dataBuf[z+1] & 255) << 8) | ((int)(dataBuf[z+2] & 255) << 16) | ((int)(dataBuf[z+3] & 255) << 24);
        z = z + 4;
    }

    //printf("Finish READ\n");
    printMap();
}

void initSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    int rv = IMG_Init(IMG_INIT_PNG);
    if ((rv & IMG_INIT_PNG) != IMG_INIT_PNG) {
        fprintf(stderr, "Error initializing IMG: %s\n", IMG_GetError());
        exit(EXIT_FAILURE);
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "Error initializing TTF: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }
}

void handleKeyDown(SDL_KeyboardEvent* event)
{
    // ignore repeat events if key is held down
    if (event->repeat)
        return;

    if (event->keysym.scancode == SDL_SCANCODE_Q || event->keysym.scancode == SDL_SCANCODE_ESCAPE)
        shouldExit = true;

    if (event->keysym.scancode == SDL_SCANCODE_UP || event->keysym.scancode == SDL_SCANCODE_W){
        strcpy(buf,"w\0");
        Rio_writen(clientfd, buf, strlen(buf));
        readMap();
    }

    if (event->keysym.scancode == SDL_SCANCODE_DOWN || event->keysym.scancode == SDL_SCANCODE_S){
        strcpy(buf,"s\0");
        Rio_writen(clientfd, buf, strlen(buf));
        readMap();
    }

    if (event->keysym.scancode == SDL_SCANCODE_LEFT || event->keysym.scancode == SDL_SCANCODE_A){
        strcpy(buf,"a\0");
        Rio_writen(clientfd, buf, strlen(buf));
        readMap();
    }

    if (event->keysym.scancode == SDL_SCANCODE_RIGHT || event->keysym.scancode == SDL_SCANCODE_D){
        strcpy(buf,"d\0");
        Rio_writen(clientfd, buf, strlen(buf));
        readMap();
    }


}

void processInputs()
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				shouldExit = true;
				break;

            case SDL_KEYDOWN:
                handleKeyDown(&event.key);
				break;

			default:
				break;
		}
	}
}

void drawGrid(SDL_Renderer* renderer, SDL_Texture* grassTexture, SDL_Texture* tomatoTexture, SDL_Texture* playerTexture, SDL_Texture* clientTexture)
{
    SDL_Rect dest;
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            dest.x = 64 * i;
            dest.y = 64 * j + HEADER_HEIGHT;
            SDL_Texture* texture;
            if(map[i][j] == 'G'){
                texture = grassTexture;
            }else if(map[i][j] == 'T'){
                texture = tomatoTexture;
            }else if(map[i][j] == '@'){
                texture = clientTexture;
            }else if(map[i][j] == 'P'){
                texture = playerTexture;
            }else{
                texture = grassTexture;
            }
            SDL_QueryTexture(texture, NULL, NULL, &dest.w, &dest.h);
            SDL_RenderCopy(renderer, texture, NULL, &dest);
        }
    }
}

void drawUI(SDL_Renderer* renderer)
{
    // largest score/level supported is 2147483647
    char scoreStr[18];
    char levelStr[18];
    char highestStr[25];
    char* scoreboard = "Scoreboard";
    char scoreboardScores[11];

    sprintf(scoreStr, "Score: %d", score);
    sprintf(levelStr, "Level: %d", level);
    if(hasTop == 1){
        sprintf(highestStr, "Highest Scorer!");
    }else{
        sprintf(highestStr, "Score to Beat: %d", highScore);
    }

    SDL_Color white = {255, 255, 255};
    SDL_Surface* scoreSurface = TTF_RenderText_Solid(font, scoreStr, white);
    SDL_Texture* scoreTexture = SDL_CreateTextureFromSurface(renderer, scoreSurface);

    SDL_Surface* levelSurface = TTF_RenderText_Solid(font, levelStr, white);
    SDL_Texture* levelTexture = SDL_CreateTextureFromSurface(renderer, levelSurface);

    SDL_Surface* highestSurface = TTF_RenderText_Solid(font, highestStr, white);
    SDL_Texture* highestTexture = SDL_CreateTextureFromSurface(renderer, highestSurface);

    SDL_Surface* scoreboardSurface = TTF_RenderText_Solid(font, scoreboard, white);
    SDL_Texture* scoreboardTexture = SDL_CreateTextureFromSurface(renderer, scoreboardSurface);

    SDL_Rect scoreDest;
    TTF_SizeText(font, scoreStr, &scoreDest.w, &scoreDest.h);
    scoreDest.x = 0;
    scoreDest.y = 0;

    SDL_Rect levelDest;
    TTF_SizeText(font, levelStr, &levelDest.w, &levelDest.h);
    levelDest.x = GRID_DRAW_WIDTH - levelDest.w;
    levelDest.y = 0;

    SDL_Rect highestDest;
    TTF_SizeText(font, highestStr, &highestDest.w, &highestDest.h);
    highestDest.x = GRID_DRAW_WIDTH/2 - highestDest.w/2;
    highestDest.y = 0;

    SDL_Rect scoreboardDest;
    TTF_SizeText(font, scoreboard, &scoreboardDest.w, &scoreboardDest.h);
    scoreboardDest.x = GRID_DRAW_WIDTH - scoreboardDest.w;
    scoreboardDest.y = 64;

    SDL_Surface* scoreboardScoresSurface; 
    SDL_Texture* scoreboardScoresTexture;
    SDL_Rect scoreboardScoresDest;
    for(int i = 0; i < 8; i++){
        if(scores[i] != -1){
            sprintf(scoreboardScores, "P%d: %d", i + 1, scores[i]);
        }else{
            sprintf(scoreboardScores, "P%d: ---", i + 1);
        }
        scoreboardScoresSurface = TTF_RenderText_Solid(font, scoreboardScores, white);
        scoreboardScoresTexture = SDL_CreateTextureFromSurface(renderer, scoreboardScoresSurface);
        TTF_SizeText(font, scoreboardScores, &scoreboardScoresDest.w, &scoreboardScoresDest.h);
        scoreboardScoresDest.x = GRID_DRAW_HEIGHT;
        scoreboardScoresDest.y = 128 + (i * 64);
        SDL_RenderCopy(renderer, scoreboardScoresTexture, NULL, &scoreboardScoresDest);
        SDL_FreeSurface(scoreboardScoresSurface);
        SDL_DestroyTexture(scoreboardScoresTexture);
    }
    

    SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreDest);
    SDL_RenderCopy(renderer, levelTexture, NULL, &levelDest);
    SDL_RenderCopy(renderer, highestTexture, NULL, &highestDest);
    SDL_RenderCopy(renderer, scoreboardTexture, NULL, &scoreboardDest);

    SDL_FreeSurface(scoreSurface);
    SDL_DestroyTexture(scoreTexture);

    SDL_FreeSurface(levelSurface);
    SDL_DestroyTexture(levelTexture);

    SDL_FreeSurface(highestSurface);
    SDL_DestroyTexture(highestTexture);

    SDL_FreeSurface(scoreboardSurface);
    SDL_DestroyTexture(scoreboardTexture);
    
}

int main(int argc, char* argv[])
{
    
    host = argv[1];
    port = argv[2];
    if(argc < 3){
        printf("Usage: client <server ip> <server port>\n");
        return 0;
    }

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);


    srand(time(NULL));

    initSDL();

    font = TTF_OpenFont("resources/Burbank-Big-Condensed-Bold-Font.otf", HEADER_HEIGHT);
    if (font == NULL) {
        fprintf(stderr, "Error loading font: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_Window* window = SDL_CreateWindow("Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);

    if (window == NULL) {
        fprintf(stderr, "Error creating app window: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);

	if (renderer == NULL)
	{
		fprintf(stderr, "Error creating renderer: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
	}

    SDL_Texture *grassTexture = IMG_LoadTexture(renderer, "resources/grass.png");
    SDL_Texture *tomatoTexture = IMG_LoadTexture(renderer, "resources/tomato.png");
    SDL_Texture *playerTexture = IMG_LoadTexture(renderer, "resources/player.png");
    SDL_Texture *clientTexture = IMG_LoadTexture(renderer, "resources/client.png");

    Rio_readlineb(&rio, buf, MAXLINE);
    Fputs(buf, stdout);
    if(buf[0] == '~'){
        return 1;
    }
    // main game loop
    readMap();
    struct timeval tval_before, tval_after, tval_result;

    gettimeofday(&tval_before, NULL);

    while (!shouldExit) {
        
        SDL_SetRenderDrawColor(renderer, 0, 105, 6, 255);
        SDL_RenderClear(renderer);

        gettimeofday(&tval_after, NULL);
        timersub(&tval_after, &tval_before, &tval_result);
        if(tv2dbl(tval_result) > 0.1 && !(tv2dbl(tval_result) < 0)){
            
            gettimeofday(&tval_before, NULL);
            strcpy(buf,"*\0");
            Rio_writen(clientfd, buf, strlen(buf));
            readMap();
        }else{
            processInputs();
        }
        drawGrid(renderer, grassTexture, tomatoTexture, playerTexture, clientTexture);
        drawUI(renderer);

        SDL_RenderPresent(renderer);

        SDL_Delay(16); // 16 ms delay to limit display to 60 fps
        
    
    }

    Close(clientfd); 
        
    // clean up everything
    SDL_DestroyTexture(grassTexture);
    SDL_DestroyTexture(tomatoTexture);
    SDL_DestroyTexture(playerTexture);
    SDL_DestroyTexture(clientTexture);

    TTF_CloseFont(font);
    TTF_Quit();

    IMG_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    exit(0);
}
