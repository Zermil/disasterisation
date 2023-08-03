#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <SDL2/SDL.h>

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
#define IGNORE_DECIMAL(x) ((f32)((s32)(x)))

#define FPS 60
#define MS_PER_FRAME (1000/FPS)
#define WIDTH 1280
#define HEIGHT 720

#define RECT_RES 40
#define RECT_ROWS (WIDTH / RECT_RES)
#define RECT_COLS (HEIGHT / RECT_RES)
#define CIRCLE_RADIUS 20

#define internal static
#define global static

typedef uint32_t u32;
typedef uint8_t u8;

typedef int32_t s32;

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

// @ToDo: This could be in some struct.
global bool mouse_held = false;
global s32 line_index = -1;

internal inline Vec2f vec2f_get_direction(Line line)
{
    return {
        line.p1.x - line.p0.x,
        line.p1.y - line.p0.y
    };
}

internal u32 line_check_intersections(Line *lines, size_t length, Line other)
{
    u32 intersections = 0;
    for (size_t i = 0; i < length; ++i) {
        Vec2f As = lines[i].p0;
        Vec2f Ad = vec2f_get_direction(lines[i]);

        Vec2f Bs = other.p0;
        Vec2f Bd = vec2f_get_direction(other);

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

internal void rasterize_shape(Line *lines, size_t length, SDL_Rect *rects, SDL_Rect *filled_rects)
{
    memset(rects, 0, RECT_ROWS*RECT_COLS*sizeof(SDL_Rect));
    memset(filled_rects, 0, RECT_ROWS*RECT_COLS*sizeof(SDL_Rect));
    
    SDL_Rect rect = {0};
    rect.w = rect.h = RECT_RES;
    
    for (u32 row = 0; row < RECT_ROWS; ++row) {
        for (u32 col = 0; col < RECT_COLS; ++col) {
            f32 x = row + 0.5f;
            f32 y = col + 0.5f;
            Line other = {{x, y}, {x - 1.0f, y}};
            
            rect.y = col * rect.h;
            rect.x = row * rect.w;
                        
            // @ToDo: Add more fill rules to see how they work on different shapes.
            if (line_check_intersections(lines, length, other) % 2 != 0) {
                ARRAY_AT(filled_rects, row, col) = rect;
            } else {
                ARRAY_AT(rects, row, col) = rect;
            }
        }
    }
}

internal s32 get_index_of_selected_origin(s32 mouse_x, s32 mouse_y, Line *lines, s32 length)
{
    for (s32 i = 0; i < length; ++i) {
        s32 x = (s32) (lines[i].p0.x * RECT_RES) - CIRCLE_RADIUS;
        s32 y = (s32) (lines[i].p0.y * RECT_RES) - CIRCLE_RADIUS;
        s32 w = 2*CIRCLE_RADIUS;
        
        if ((mouse_x >= x && mouse_x <= x + w) &&
            (mouse_y >= y && mouse_y <= y + w))
        {
            return(i);
        }
    }

    return(-1);
}

internal void render_draw_circle(SDL_Renderer *renderer, u32 cx, u32 cy, u32 r, u8 red, u8 green, u8 blue)
{
    u32 x = r;
    u32 y = 0;
    s32 p = 1 - r;
    
    SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
    SDL_RenderDrawLine(renderer, cx-r, cy, cx+r, cy);
    
    while (x >= y) {
        y += 1;
        
        if (p <= 0) {
            p += 2*y + 1;
        } else {
            x -= 1;
            p += 2*y - 2*x + 1;
        }

        SDL_RenderDrawLine(renderer, cx+x, cy-y, cx-x, cy-y);
        SDL_RenderDrawLine(renderer, cx+x, cy+y, cx-x, cy+y);
        SDL_RenderDrawLine(renderer, cx+y, cy-x, cx-y, cy-x);
        SDL_RenderDrawLine(renderer, cx+y, cy+x, cx-y, cy+x);
    }
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

    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    
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

    // @Note: This is a placeholder for now, just to start
    // with some basic points.
    Line lines[3] = {0};
    lines[0].p0 = {3, 3};
    lines[1].p0 = {10, 4};
    lines[2].p0 = {8, 8};
    lines[0].p1 = {lines[1].p0.x, lines[1].p0.y};
    lines[1].p1 = {lines[2].p0.x, lines[2].p0.y};
    lines[2].p1 = {lines[0].p0.x, lines[0].p0.y};
    
    rasterize_shape(lines, ARRAY_LEN(lines), rects, filled_rects);
    
    Render_Ctx context = create_render_context(WIDTH, HEIGHT, "A Window");
    bool should_quit = false;
    u32 current_time = 0;
    u32 previous_time = SDL_GetTicks();
    
    while (!should_quit) {
        current_time = SDL_GetTicks();
        u32 time_elapsed = current_time - previous_time;
        u32 time_to_wait = MS_PER_FRAME - time_elapsed;
        previous_time = current_time;
        
        SDL_Event e = {0};
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: {
                    should_quit = true;
                } break;

                case SDL_MOUSEBUTTONDOWN: {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        mouse_held = true;
                        line_index = get_index_of_selected_origin(e.button.x, e.button.y, lines, ARRAY_LEN(lines));
                    }
                } break;

                case SDL_MOUSEBUTTONUP: {
                    if (e.button.button == SDL_BUTTON_LEFT) {
                        mouse_held = false;
                        line_index = -1;
                    }
                } break;

                case SDL_MOUSEMOTION: {
                    if (mouse_held && line_index != -1) {
                        f32 x = IGNORE_DECIMAL(((f32) e.motion.x / (f32) WIDTH) * RECT_ROWS);
                        f32 y = IGNORE_DECIMAL(((f32) e.motion.y / (f32) HEIGHT) * RECT_COLS);
                        
                        if ((x > 0 && x < RECT_ROWS) && (y > 0 && y < RECT_COLS)) {
                            // @Note: Simple way to loop back when line_index = 0.
                            s32 connected_line_index = line_index - 1 == -1 ? ARRAY_LEN(lines) - 1 : line_index - 1;
                            
                            lines[line_index].p0.x = x;
                            lines[line_index].p0.y = y;
                            lines[connected_line_index].p1.x = x;
                            lines[connected_line_index].p1.y = y;
                        
                            rasterize_shape(lines, ARRAY_LEN(lines), rects, filled_rects);
                        }
                    }
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

        for (u32 i = 0; i < ARRAY_LEN(lines); ++i) {
            u32 x1 = (u32) (lines[i].p0.x * RECT_RES);
            u32 y1 = (u32) (lines[i].p0.y * RECT_RES);
            u32 x2 = (u32) (lines[i].p1.x * RECT_RES);
            u32 y2 = (u32) (lines[i].p1.y * RECT_RES);

            SDL_SetRenderDrawColor(context.renderer, 255, 0, 0, 255);
            SDL_RenderDrawLine(context.renderer, x1, y1, x2, y2);

            render_draw_circle(context.renderer, x1, y1, CIRCLE_RADIUS, 255, 0, 0);
        }

        SDL_RenderPresent(context.renderer);
        if (time_to_wait > 0 && time_to_wait < MS_PER_FRAME) {
            SDL_Delay(time_to_wait);
        }
    }

    destroy_render_context(&context);

    return 0;
}
