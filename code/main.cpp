#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <SDL2/SDL.h>

// @ToDo: Add functionality to move the triangle points.
// @ToDo: Add more fill rules to see how they work on different shapes.

#define UNUSED(x) ((void)(x))

#define ERROR_EXIT(err, msg, ...)                   \
    do {                                            \
        if ((err)) {                                \
            fprintf(stderr, (msg), __VA_ARGS__);    \
            exit(1);                                \
        }                                           \
    } while(0)                                      \

#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(arr[0]))
#define ARRAY_AT(arr, row, col) ((arr)[RECT_COLS * (row) + (col)])
#define MOVE_ARRAY_ELEMENT(from, to, x, y)                      \
    do {                                                        \
        ARRAY_AT((to), (x), (y)) = ARRAY_AT((from), (x), (y));  \
        ARRAY_AT((from), (x), (y)) = {0};                       \
    } while (0)                                                 \

#define WIDTH 1280
#define HEIGHT 720

#define RECT_RES 40
#define RECT_ROWS WIDTH / RECT_RES
#define RECT_COLS HEIGHT / RECT_RES

#define internal static

typedef uint32_t u32;
typedef float f32;

struct Render_Ctx {
    SDL_Window *window;
    SDL_Renderer *renderer;
};

struct Vec2f {
    f32 x;
    f32 y;
};

struct Line {
    Vec2f p0;
    Vec2f p1;
};

Vec2f line_get_direction(Line line)
{
    Vec2f dir = {0};
    dir.x = line.p1.x - line.p0.x;
    dir.y = line.p1.y - line.p0.y;
    
    return(dir);
}

u32 line_check_intersections(Line *lines, size_t len, Line other)
{
    u32 intersections = 0;
    for (size_t i = 0; i < len; ++i) {
        Vec2f As = lines[i].p0;
        Vec2f Ad = line_get_direction(lines[i]);

        Vec2f Bs = other.p0;
        Vec2f Bd = line_get_direction(other);

        // @Note: For more information read the supplimentary paper 'Lines intersection.pdf', while trying to get
        // 'inspired' for this project I also found this amazing implementation, which might be helpful to some.
        //
        // https://github.com/leddoo/edu-vector-graphics/blob/master/src/main.rs
        f32 det = -Ad.x*Bd.y + Ad.y*Bd.x;
        if (det == 0.0f) continue;

        f32 i_det = 1.0f / det;
        f32 t = i_det * (-Bd.y*(Bs.x - As.x) + Bd.x*(Bs.y - As.y));
        f32 u = i_det * (-Ad.y*(Bs.x - As.x) + Ad.x*(Bs.y - As.y));

        // @Note: Our 'u >= 0' means that we don't care how much we stretch the 'other' line/ray.
        if (u >= 0.0f && (t >= 0.0f && t <= 1.0f)) intersections += 1;
    }

    return(intersections);
}

internal Render_Ctx create_render_context(u32 width, u32 height, const char *window_title)
{
    Render_Ctx context = {0};
    
    ERROR_EXIT(SDL_Init(SDL_INIT_VIDEO) != 0, "[ERROR]: Could not initialize SDL2 -> %s\n", SDL_GetError());

    context.window = SDL_CreateWindow(window_title,
                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      width, height,
                                      SDL_WINDOW_SHOWN);
    ERROR_EXIT(context.window == 0, "[ERROR]: Could not create SDL2 window");
    
    context.renderer = SDL_CreateRenderer(context.window, -1, SDL_RENDERER_ACCELERATED);
    ERROR_EXIT(context.renderer == 0, "[ERROR]: Could not create SDL2 renderer");
    
    return(context);
}

internal void destroy_render_context(Render_Ctx *ctx)
{
    SDL_DestroyWindow(ctx->window);
    SDL_DestroyRenderer(ctx->renderer);

    SDL_Quit();
}

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    
    SDL_Rect rects[RECT_ROWS * RECT_COLS] = {0};
    SDL_Rect filled_rects[RECT_ROWS * RECT_COLS] = {0};

    Line lines[3] = {0};
    lines[0].p0 = {3, 3};
    lines[0].p1 = {10, 4};

    lines[1].p0 = {10, 4};
    lines[1].p1 = {8, 8};

    lines[2].p0 = {8, 8};
    lines[2].p1 = {3, 3};
    
    // @Note: Create board
    for (u32 row = 0; row < RECT_ROWS; ++row) {
        for (u32 col = 0; col < RECT_COLS; ++col) {
            SDL_Rect rect = {0};
            rect.w = rect.h = RECT_RES;
            rect.x = row * rect.w;
            rect.y = col * rect.h;

            ARRAY_AT(rects, row, col) = rect;
        }
    }

    // @Note: Begin rasterisation
    for (u32 row = 0; row < RECT_ROWS; ++row) {
        for (u32 col = 0; col < RECT_COLS; ++col) {
            f32 x = row + 0.5f;
            f32 y = col + 0.5f;
            Line other = {{x, y}, {x - 1.0f, y}};
            
            if (line_check_intersections(lines, ARRAY_LEN(lines), other) % 2 != 0) {
                MOVE_ARRAY_ELEMENT(rects, filled_rects, row, col);
            }
        }
    }
    
    Render_Ctx context = create_render_context(WIDTH, HEIGHT, "A Window");
    bool should_quit = false;
    
    while (!should_quit) {
        SDL_Event e = {0};
        
        if (SDL_WaitEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: {
                    should_quit = true;
                } break;
            }
        }

        SDL_SetRenderDrawColor(context.renderer, 18, 18, 18, 255);
        SDL_RenderClear(context.renderer);

        // @Note: Banana-cakes
        for (u32 i = 0; i < RECT_ROWS * RECT_COLS; ++i) {
            SDL_SetRenderDrawColor(context.renderer, 80, 80, 80, 255);
            SDL_RenderDrawRect(context.renderer, &rects[i]);
            
            SDL_SetRenderDrawColor(context.renderer, 0, 120, 0, 255);
            SDL_RenderFillRect(context.renderer, &filled_rects[i]);
        }

        for (u32 i = 0; i < 3; ++i) {
            u32 x1 = (u32) ((lines[i].p0.x) * RECT_RES);
            u32 y1 = (u32) ((lines[i].p0.y) * RECT_RES);
            u32 x2 = (u32) ((lines[i].p1.x) * RECT_RES);
            u32 y2 = (u32) ((lines[i].p1.y) * RECT_RES);
            
            SDL_SetRenderDrawColor(context.renderer, 255, 0, 0, 255);
            SDL_RenderDrawLine(context.renderer, x1, y1, x2, y2);
        }
        
        SDL_RenderPresent(context.renderer);
    }

    destroy_render_context(&context);
    
    return 0;
}
