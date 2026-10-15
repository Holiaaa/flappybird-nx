#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <random>
#include <time.h>
#include <stdlib.h>

#include <switch.h>

// made by Téo JAUFFRET (Holiaaa)

// version 2.0 on Nintendo Switch !

class Pipe {
public:
    int posX = 0;
    int posY = 0;
};

void renderScore(SDL_Renderer *renderer, TTF_Font *font, const char *scoreText, SDL_Rect scoreRect) {
    if (!font || !scoreText || scoreText[0] == '\0') return;

    // 1. Créer la surface à la taille native de la police
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface *textSurface = TTF_RenderText_Blended(font, scoreText, textColor);
    if (!textSurface) return;

    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        return;
    }

    // 2. Calculer le ratio pour que le texte rentre parfaitement dans scoreRect sans déformation
    int textW = textSurface->w;
    int textH = textSurface->h;
    
    float ratioX = (float)scoreRect.w / textW;
    float ratioY = (float)scoreRect.h / textH;
    float scale = (ratioX < ratioY) ? ratioX : ratioY;

    // Si le texte est plus petit que la boîte, on peut choisir de le garder à sa taille réelle (scale = 1.0)
    if (scale > 1.0f) scale = 1.0f; 

    int finalW = (int)(textW * scale);
    int finalH = (int)(textH * scale);

    // 3. Centrer le texte dans le scoreRect donné
    SDL_Rect destRect = {
        scoreRect.x + (scoreRect.w - finalW) / 2,
        scoreRect.y + (scoreRect.h - finalH) / 2,
        finalW,
        finalH
    };

    // 4. Dessiner et nettoyer la mémoire locale volatile (Très rapide)
    SDL_RenderCopy(renderer, textTexture, NULL, &destRect);

    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}


int main(int argc, char *argv[]) {
    srand(time(NULL));
    romfsInit();

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
        return -1;
    }

    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();
    
    Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);

    SDL_Window* window = SDL_CreateWindow("flappybird NX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    bool running = true;
    SDL_Event event;

    //const Uint8 *state = SDL_GetKeyboardState(NULL);

    int playerYpos = 100;
    float playerSpeed = 0;

    Pipe groundPipes[4];
    Pipe airPipes[4];

    SDL_Texture *backgroundTexture = IMG_LoadTexture(renderer, "romfs:/assets/background.png");
    if (!backgroundTexture) {
        std::cout << "Error while creating texture for background. error : " << IMG_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        Mix_Quit();
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
    }

    SDL_Texture *pipe0Texture = IMG_LoadTexture(renderer, "romfs:/assets/sprites/pipe0.png");
    if (!pipe0Texture) {
        std::cout << "Error while creating texture for pipe. error : " << IMG_GetError() << std::endl;
        SDL_DestroyTexture(backgroundTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        Mix_Quit();
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
    }

    SDL_Texture *pipe1Texture = IMG_LoadTexture(renderer, "romfs:/assets/sprites/pipe1.png");
    if (!pipe1Texture) {
        std::cout << "Error while creating texture for pipe. error : " << IMG_GetError() << std::endl;
        SDL_DestroyTexture(backgroundTexture);
        SDL_DestroyTexture(pipe0Texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        Mix_Quit();
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
    }

    SDL_Texture *birdTexture = IMG_LoadTexture(renderer, "romfs:/assets/sprites/bird_0.png");
    if (!birdTexture) {
        std::cout << "Error while creating texture for bird. error : " << IMG_GetError() << std::endl;
        SDL_DestroyTexture(backgroundTexture);
        SDL_DestroyTexture(pipe0Texture);
        SDL_DestroyTexture(pipe1Texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        Mix_Quit();
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
    }

    Mix_Chunk* jumpsound = Mix_LoadWAV("romfs:/assets/sounds/jump.ogg");
    if (jumpsound == NULL) {
        printf("Mix_LoadWAV failed: %s\n", Mix_GetError());
        return -1;
    }

    Mix_Chunk* coinsound = Mix_LoadWAV("romfs:/assets/sounds/pickupCoin.wav");
    if (coinsound == NULL) {
        printf("Mix_LoadWAV failed: %s\n", Mix_GetError());
        return -1;
    }

    Mix_Chunk* explosion = Mix_LoadWAV("romfs:/assets/sounds/explosion.wav");
    if (explosion == NULL) {
        printf("Mix_LoadWAV failed: %s\n", Mix_GetError());
        return -1;
    }

    // [Dans le main, avant la boucle while]
    TTF_Font *gameFont = TTF_OpenFont("romfs:/assets/font/font.ttf", 48);
    if (!gameFont) {
        std::cout << "Impossible de charger la police ! Erreur : " << TTF_GetError() << std::endl;
        return -1;
    }

    Mix_AllocateChannels(16);

    int birdId = 0;

    int score = 0;
    char scoreBuffer[20];
    int scoreYpos = 50;
    sprintf(scoreBuffer, "%d", score);

    int gameoverYpos = -80;

    double angle = 0.0;
    double angleSpeed = 0;
    bool collideWithAPipe = false;

    bool explosionBool = true;

    int spacePressed = 0;

    bool menu = true;
    bool skinmenu = false;

    bool skinPressed = false;

    bool over30 = false;

    SDL_Color white = {255, 255, 255};

    for (int i = 0; i < 4; i++) {
        groundPipes[i].posX = 300*i + 800;
        groundPipes[i].posY = 320 + (std::rand() % 150);
    }

    for (int i = 0; i < 4; i++) {
        airPipes[i].posX = groundPipes[i].posX;
        airPipes[i].posY = groundPipes[i].posY - 480;
    }
    
    while (running) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        SDL_SetRenderDrawColor(renderer, 14, 124, 247, 255);
        SDL_RenderClear(renderer);

        SDL_RenderCopy(renderer, backgroundTexture, NULL, NULL);

        // loop start here

        if (skinmenu) {
            SDL_Rect skinMenuTitle = {500, 60, 250, 100};
            renderScore(renderer, gameFont, "Skin Menu", skinMenuTitle);

            SDL_Rect rightArrow = {680, 300, 40, 40};
            renderScore(renderer, gameFont,">", rightArrow);

            SDL_Rect leftArrow = {528, 300, 40, 40};
            renderScore(renderer, gameFont,"<", leftArrow);

            char buffer[64]; // Crée un tableau de caractères temporaire
            snprintf(buffer, sizeof(buffer), "romfs:/assets/sprites/bird_%d.png", birdId); // Formate la chaîne avec la variable
            birdTexture = IMG_LoadTexture(renderer, buffer);
            if (!birdTexture) {
                std::cout << "Error while creating texture for bird. error : " << IMG_GetError() << std::endl;
                SDL_DestroyTexture(backgroundTexture);
                SDL_DestroyTexture(pipe0Texture);
                SDL_DestroyTexture(pipe1Texture);
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                Mix_Quit();
                IMG_Quit();
                TTF_Quit();
                SDL_Quit();
            }

            SDL_Rect players = {600, 300, 50, 40};
            SDL_Point centers = {players.w /2, players.h/2};
            SDL_RenderCopyEx(renderer, birdTexture, NULL, &players, angle, &centers, SDL_FLIP_NONE);

            /*if (state[SDL_SCANCODE_ESCAPE]) {
                skinmenu = false;
            }*/

            if (kDown & HidNpadButton_X) {
                skinmenu = false;
            }

            if (over30) {
                SDL_Rect secretSkinText = {520, 540, 150, 75};
                renderScore(renderer, gameFont,"Secret skin unlocked!", secretSkinText);
            }

            if (kDown & HidNpadButton_Left) {
                if (!skinPressed) {
                    birdId--;
                    if (birdId < 0) {
                        birdId = 0;
                    }
                    skinPressed = true;
                }
            } else if (kDown & HidNpadButton_Right) {
                if (!skinPressed) {
                    if (over30) {
                        birdId++;
                        if (birdId > 5) {
                            birdId = 5;
                        }
                        skinPressed = true;
                    } else {
                        birdId++;
                        if (birdId > 4) {
                            birdId = 4;
                        }
                        skinPressed = true;
                    }
                }
            }

            if (!(kDown & HidNpadButton_Left) && !(kDown & HidNpadButton_Right)) {
                skinPressed = false;
            }

            SDL_RenderPresent(renderer);
            continue;
        }

        if (menu) {
            SDL_Rect menuRect = {504, 240, 250, 100};
            renderScore(renderer, gameFont,"FlappyBird", menuRect);

            SDL_Rect spaceRect = {506, 320, 250, 30};
            renderScore(renderer, gameFont,"Press A", spaceRect);

            SDL_Rect skinRect = {505, 340, 250, 30};
            renderScore(renderer, gameFont,"Press + for SKIN", skinRect);

            if (kDown & HidNpadButton_A) {
                menu = false;
            } else if (kDown & HidNpadButton_Plus) {
                skinmenu = true;
            }

            SDL_RenderPresent(renderer);
            continue;
        }

        SDL_Rect player = {100, playerYpos, 50, 40};

        playerYpos += playerSpeed;
        playerSpeed += 0.375;

        if (kDown & HidNpadButton_A) {
            if (!spacePressed && !collideWithAPipe) {
                playerSpeed = -5.6;
                angleSpeed = 1.5;
                angle = -(player.x / 4) * 128;
                if (Mix_PlayChannel(-1, jumpsound, 0) == -1) {
                    printf("Mix_PlayChannel failed: %s\n", Mix_GetError());
                    return -1;
                }
        
                spacePressed = 1;
            }
        } else {
            spacePressed = 0;
        }

        if (player.y < 0) {
            playerSpeed = 0;
            playerYpos = 0;
        }

        if (player.y > 600) {
            collideWithAPipe = true;
        }

        if (angle > -60 && angle < 60) {
            angle += angleSpeed;
            angleSpeed += 0.015;
        } else {
            if (angle >= 60) {
                angle = 60;
            }
            else if (angle <= -60) {
                angle = -59;
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_Point center = {player.w /2, player.h/2};
        SDL_RenderCopyEx(renderer, birdTexture, NULL, &player, angle, &center, SDL_FLIP_NONE);
        //SDL_RenderFillRect(renderer, &player);

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        int hauteur_y = 0;
        for (Pipe& pipe : groundPipes) {
            if (pipe.posX < -50) {
                pipe.posX = 350 + 800;
                pipe.posY = 350 + (std::rand() % 150);
                hauteur_y = pipe.posY;
            } else {
                if (!collideWithAPipe) {
                    pipe.posX -= 2;
                }
                SDL_Rect _pipe = {pipe.posX, pipe.posY, 60, 320};
                SDL_Rect _pipe2 = {pipe.posX, pipe.posY+300, 60, 320};
                //SDL_RenderFillRect(renderer, &_pipe);
                SDL_RenderCopy(renderer, pipe0Texture, NULL, &_pipe);
                SDL_RenderCopy(renderer, pipe1Texture, NULL, &_pipe2);


                if (SDL_HasIntersection(&player, &_pipe)) {
                    collideWithAPipe = true;
                    if (explosionBool) {
                        explosionBool = false;
                        if (Mix_PlayChannel(-1, explosion, 0) == -1) {
                            printf("Mix_PlayChannel failed: %s\n", Mix_GetError());
                            return -1;
                        }
                    }
                }
            }
        }

        for (Pipe& pipe : airPipes) {
            if (pipe.posX < -50) {
                pipe.posX = 350 + 800;
                pipe.posY = hauteur_y - 480;
            } else {
                if (!collideWithAPipe) {
                    pipe.posX -= 2;
                }
                SDL_Rect _pipe = {pipe.posX, pipe.posY, 60, 320};
                SDL_Rect _pipe2 = {pipe.posX, pipe.posY-300, 60, 320};
                //SDL_RenderFillRect(renderer, &_pipe);
                SDL_RenderCopy(renderer, pipe1Texture, NULL, &_pipe);
                SDL_RenderCopy(renderer, pipe0Texture, NULL, &_pipe2);

                if (SDL_HasIntersection(&player, &_pipe)) {
                    collideWithAPipe = true;
                    if (explosionBool) {
                        explosionBool = false;
                        if (Mix_PlayChannel(-1, explosion, 0) == -1) {
                            printf("Mix_PlayChannel failed: %s\n", Mix_GetError());
                            return -1;
                        }
                    }
                }
            }
        }

        for (Pipe& pipe : groundPipes) {
            if (pipe.posX-20 == player.x) {
                score++;
                if (score >= 20) {
                    over30 = true;
                }
                if (Mix_PlayChannel(-1, coinsound, 0) == -1) {
                    printf("Mix_PlayChannel failed: %s\n", Mix_GetError());
                    return -1;
                }
                sprintf(scoreBuffer, "%d", score);
            }
        }

        SDL_Rect scoreRect = {616, scoreYpos, 50, 50};
        if (collideWithAPipe) {
            if (scoreYpos < 300) {
                scoreYpos = scoreYpos + 2;
            }
            SDL_Rect gameoverRect = {530, gameoverYpos, 200, 100};
            if (collideWithAPipe) {
                if (gameoverYpos < 195) {
                    gameoverYpos = gameoverYpos + 2.4;
                }
            }
            renderScore(renderer, gameFont,"Game Over", gameoverRect);

            if (kDown & HidNpadButton_X) {
                playerYpos = 100;
                playerSpeed = 0;
                score = 0;
                sprintf(scoreBuffer, "%d", score);
                scoreYpos = 50;
                gameoverYpos = -80;
                angle = 0.0;
                angleSpeed = 0;
                collideWithAPipe = false;

                for (int i = 0; i < 4; i++) {
                    groundPipes[i].posX = 300*i + 800;
                    groundPipes[i].posY = 320 + (std::rand() % 150);
                }

                for (int i = 0; i < 4; i++) {
                    airPipes[i].posX = groundPipes[i].posX;
                    airPipes[i].posY = groundPipes[i].posY - 480;
                }

                menu = true;
            }

            if (kDown & HidNpadButton_A) {
                // Reset game state
                playerYpos = 100;
                playerSpeed = 0;
                score = 0;
                sprintf(scoreBuffer, "%d", score);
                scoreYpos = 50;
                gameoverYpos = -80;
                angle = 0.0;
                angleSpeed = 0;
                collideWithAPipe = false;

                for (int i = 0; i < 4; i++) {
                    groundPipes[i].posX = 300*i + 800;
                    groundPipes[i].posY = 320 + (std::rand() % 150);
                }

                for (int i = 0; i < 4; i++) {
                    airPipes[i].posX = groundPipes[i].posX;
                    airPipes[i].posY = groundPipes[i].posY - 480;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 14, 124, 247, 255);
        renderScore(renderer, gameFont,scoreBuffer, scoreRect);

        SDL_RenderPresent(renderer);
        SDL_Delay(7);
    }

    Mix_FreeChunk(jumpsound);
    SDL_DestroyTexture(backgroundTexture);
    SDL_DestroyTexture(pipe0Texture);
    SDL_DestroyTexture(pipe1Texture);
    SDL_DestroyTexture(birdTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    Mix_CloseAudio();
    Mix_Quit();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    romfsExit();
    
    return 0;
}