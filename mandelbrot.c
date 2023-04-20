#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>

#include <SDL.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define NUM_THREADS 6

struct Coord {
    double x;
    double y;
};

struct Game {
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Texture *offscreen;
    struct Coord topleft_corner;
    double step;
    uint64_t last_it;
    uint64_t current_it;
    char **colors;
    int rendered;
    int remake_colors;
};

void checkerr(void *p, const char *message) {
    if (p == NULL) {
        fputs(message, stderr);
        exit(1);
    }
}


void init_SDL(struct Game *game) {
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
        exit(1);
    }

    game->window = SDL_CreateWindow(
        "Mandelbrot's Set",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0
    );
    checkerr(game->window, "Error initializing SDL window");
    game->renderer = SDL_CreateRenderer(game->window, -1, SDL_RENDERER_ACCELERATED);
    checkerr(game->renderer, "Error initializing SDL renderer");
}

void hsv_to_rgb(double h, double s, double v, char *color) {
    double c, x, m;
    c = v * s;
    x = c * (1.0 - fabs(fmod(h/60, 2) - 1));
    m = v - c;

    double r, g, b;
    if (h < 60) {
        r = c; g = x; b = 0;

    } else if (h < 120) {
        r = x; g = c; b = 0;

    } else if (h < 180) {
        r = 0; g = c; b = x;

    } else if (h < 240) {
        r = 0; g = x; b = c;

    } else if (h < 300) {
        r = x; g = 0; b = c;

    } else {
        r = c; g = 0; b = x;
    }

    color[0] = (r + m) * 255;
    color[1] = (g + m) * 255;
    color[2] = (b + m) * 255;
}

void make_colors(struct Game *game) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
    } else {
        for (size_t i = 0; i < game->last_it; i++) {
            free(game->colors[i]);
        }
        free(game->colors);
        game->last_it = game->current_it;
    }

    game->colors = malloc(sizeof(char *) * game->current_it);
    checkerr(game->colors, "Error allocating game colors");
    for (size_t i = 0; i < game->current_it; i++) {
        double h, s, v;
        h = (1 - ((double) i)/((double) game->current_it)) * 255;
        s = 1;
        if (i == game->current_it - 1) {
            v = 0;
        } else {
            v = 1;
        }
        game->colors[i] = malloc(3);
        checkerr(game->colors[i], "Error allocating game colors");
        hsv_to_rgb(h, s, v, game->colors[i]);
    }
}

struct Game *init(void) {
    struct Game *game = malloc(sizeof(struct Game));
    checkerr(game, "Error allocating game struct");

    init_SDL(game);

    game->offscreen = SDL_CreateTexture(
        game->renderer,
        SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );
    checkerr(game->offscreen, "Error allocating game offscreen");
    game->topleft_corner = (struct Coord) {-2.5, -1};
    game->step = ((double) 2.0) / ((double) SCREEN_HEIGHT);
    game->last_it = 100;
    game->current_it = game->last_it;
    make_colors(game);
    game->rendered = 0;
    game->remake_colors = 0;

    return game;
}

void input(struct Game *game) {
    SDL_Event event;
    int x, y;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            
            x = event.button.x;
            y = event.button.y;
            switch (event.button.button) {
            case SDL_BUTTON_LEFT:
                game->topleft_corner.x += game->step * (((double) x) - (((double) SCREEN_WIDTH) / 4.0));
                game->topleft_corner.y += game->step * (((double) y) - (((double) SCREEN_HEIGHT) / 4.0));
                game->step /= 2;
                break;

            case SDL_BUTTON_RIGHT:
                game->topleft_corner.x -= game->step * ((double) SCREEN_WIDTH) / 2.0;
                game->topleft_corner.y -= game->step * ((double) SCREEN_HEIGHT) / 2.0;
                game->step *= 2;
                break;

            }
            game->rendered = 0;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_n:
                if (game->current_it > 100) {
                    game->current_it -= 100;

                } else if (game->current_it > 10) {
                    game->current_it = 10;
                }
                break;

            case SDLK_m:
                if (game->current_it == 10) {
                    game->current_it = 100;

                } else {
                    game->current_it += 100;
                }
                break;
            }
            game->remake_colors = 1;
            game->rendered = 0;
            printf("Current it: %ld\n", game->current_it);
            break;

        case SDL_QUIT:
            exit(0);
        }        
    }
}

struct ThreadArg {
    struct Game *game;
    char *pixels;
    struct Coord left_corner;
    size_t num_rows;
};

void *render_thread(void *args) {
    struct ThreadArg *thread_args = args;

    double x, y;
    x = thread_args->left_corner.x;
    y = thread_args->left_corner.y;
    for (size_t j = 0; j < thread_args->num_rows; j++) {
        for (size_t i = 0; i < SCREEN_WIDTH; i++, (thread_args->pixels)++) {
            uint64_t it = 0;
            double zy = 0, zx = 0;
            for (; it < thread_args->game->current_it; it++) {
                double zx2, zy2;
                zx2 = (zx*zx - zy*zy) + x;
                zy2 = (zx*zy + zy*zx) + y;
                zx = zx2; zy = zy2;
                if (zx*zx + zy*zy > 4.0) {
                    it++;
                    break;
                }
            }
            it--;
            const char *color = thread_args->game->colors[it];
            *(thread_args->pixels) = color[0];
            *(++(thread_args->pixels)) = color[1];
            *(++(thread_args->pixels)) = color[2];
            x += thread_args->game->step;
        }
        x = thread_args->left_corner.x;
        y += thread_args->game->step;
    }

    return NULL;
}

void render_threaded(struct Game *game, char *pixels) {
    pthread_t threads[NUM_THREADS];
    struct ThreadArg args[NUM_THREADS];
    size_t num_rows = SCREEN_HEIGHT / NUM_THREADS;
    double y = game->topleft_corner.y;
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].game = game;
        args[i].pixels = pixels;
        printf("%p\n", pixels);
        args[i].left_corner = (struct Coord) {game->topleft_corner.x, y};
        if (i == NUM_THREADS - 1) {
            num_rows += SCREEN_HEIGHT % NUM_THREADS;
        }
        args[i].num_rows = num_rows;

        pixels += num_rows * SCREEN_WIDTH * 3;
        y += game->step * ((double) num_rows);
        pthread_create(&threads[i], NULL, render_thread, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
}

void update(struct Game *game) {
    if (game->rendered) {
        input(game);
        return;
    }

    if (game->remake_colors) {
        make_colors(game);
        game->remake_colors = 0;
    }

    char *pixels;
    int pitch;
    if (SDL_LockTexture(game->offscreen, NULL,(void **) &pixels, &pitch) < 0) {
        fprintf(stderr, "Error locking SDL texture: %s\n", SDL_GetError());
        exit(1);
    }
    render_threaded(game, pixels);
    SDL_UnlockTexture(game->offscreen);

    game->rendered = 1;
}

void draw(struct Game *game) {
    SDL_RenderCopy(game->renderer, game->offscreen, NULL, NULL);
    SDL_RenderPresent(game->renderer);
}

int main(int argc, char **argv) {
    struct Game *game = init();

    for (;;) {
        update(game);
        draw(game);
    }
}