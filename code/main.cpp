#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <SDL2/SDL.h>

typedef uint32_t u32;
typedef uint8_t  u8;
typedef int32_t  s32;
typedef float    f32;

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
#define FLOORF(x) ((f32)((s32)(x)))

#define FPS 60
#define MS_PER_FRAME (1000/FPS)
#define WIDTH 1280
#define HEIGHT 720

#define RECT_RES 20
#define RECT_ROWS (WIDTH / RECT_RES)
#define RECT_COLS (HEIGHT / RECT_RES)
#define CIRCLE_RADIUS 15
#define LINES_MAX 32

#define internal static
#define global static

struct Render_Ctx {
    SDL_Window *window;
    SDL_Renderer *renderer;
};

// @ToDo: Would be nice to convert everything to integers.
struct Vec2f {
    f32 x;
    f32 y;
};

struct Line {
    f32 x0;
    f32 y0;
    f32 x1;
    f32 y1;
};

struct Line_Array {
    Line data[LINES_MAX];
    size_t size;
};

struct Min_bb {
    u32 max_x;
    u32 min_x;    
    u32 max_y;
    u32 min_y;
};

// @ToDo: This could be in some struct.
global bool mouse_held = false;
global s32 line_index = -1;

internal inline void line_array_add(Line_Array *lines, f32 x0, f32 y0, f32 x1, f32 y1)
{
    if (lines->size == LINES_MAX) return;
   
    lines->data[lines->size].x0 = x0;
    lines->data[lines->size].x1 = x1;
    lines->data[lines->size].y0 = y0;
    lines->data[lines->size].y1 = y1;
    lines->size += 1;
}

internal inline Vec2f vec2f_get_direction(Line line)
{
    return {
        line.x1 - line.x0,
        line.y1 - line.y0
    };
}

internal bool line_check_intersections(Line line, Line other, f32 *t, f32 *u)
{
    Vec2f As = {line.x0, line.y0};
    Vec2f Ad = vec2f_get_direction(line);

    Vec2f Bs = {other.x0, other.y0};
    Vec2f Bd = vec2f_get_direction(other);

    // @Note: For more information read the supplimentary paper 'Lines intersection.pdf', while trying to get
    // 'inspired' for this project I also found this amazing implementation, which might be helpful to some.
    //
    // https://github.com/leddoo/edu-vector-graphics/blob/master/src/main.rs
    f32 det = -Ad.x*Bd.y + Ad.y*Bd.x;
    if (det == 0.0f) return(false);

    *t = (1.0f/det) * (-Bd.y*(Bs.x - As.x) + Bd.x*(Bs.y - As.y));
    *u = (1.0f/det) * (-Ad.y*(Bs.x - As.x) + Ad.x*(Bs.y - As.y));

    return(true);
}

internal Min_bb find_min_bb(Line_Array *lines)
{
    Min_bb min_bb = {0};
    min_bb.min_x = (u32) lines->data[0].x0;
    min_bb.max_x = (u32) lines->data[0].x0;
    min_bb.min_y = (u32) lines->data[0].y0;
    min_bb.max_y = (u32) lines->data[0].y0;

    // @Note: Lines are all connected no need to check
    // lines->data[i].x1/y1 as it will just be the next item.
    for (size_t i = 1; i < lines->size; ++i) {
        if (lines->data[i].x0 < min_bb.min_x) min_bb.min_x = (u32) lines->data[i].x0;
        else if (lines->data[i].x0 > min_bb.max_x) min_bb.max_x = (u32) lines->data[i].x0;
        
        if (lines->data[i].y0 < min_bb.min_y) min_bb.min_y = (u32) lines->data[i].y0;
        else if (lines->data[i].y0 > min_bb.max_y) min_bb.max_y = (u32) lines->data[i].y0;
    }

    return(min_bb);
}

// @ToDo: Add more fill rules to see how they work on different shapes.
internal void rasterize_shape(Line_Array *lines, SDL_Rect *rects, SDL_Rect *filled_rects)
{
    memset(filled_rects, 0, sizeof(SDL_Rect)*RECT_ROWS*RECT_COLS);
    
    Min_bb min_bb = find_min_bb(lines);
    f32 t, u;
    
    for (u32 row = min_bb.min_x; row < min_bb.max_x; ++row) {
        for (u32 col = min_bb.min_y; col < min_bb.max_y; ++col) {
            f32 x = row + 0.5f;
            f32 y = col + 0.5f;
            Line other = {x, y, x - 1.0f, y};
            
            u32 intersections = 0;
            for (size_t i = 0; i < lines->size; ++i) {
                if (!line_check_intersections(lines->data[i], other, &t, &u)) continue;
                
                // @Note: Our 'u >= 0' means that we don't care how much we stretch the 'other' line/ray.
                if (u >= 0.0f && (t >= 0.0f && t <= 1.0f)) intersections += 1;
            }

            if (intersections % 2 != 0) ARRAY_AT(filled_rects, row, col) = ARRAY_AT(rects, row, col);
        }
    }
}

internal s32 get_index_of_selected_origin(s32 mouse_x, s32 mouse_y, Line_Array *lines)
{
    const s32 w = 2*CIRCLE_RADIUS;
    
    for (s32 i = 0; i < (s32) lines->size; ++i) {
        s32 x = (s32) (lines->data[i].x0 * RECT_RES) - CIRCLE_RADIUS;
        s32 y = (s32) (lines->data[i].y0 * RECT_RES) - CIRCLE_RADIUS;    
        
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

    Line_Array lines = {0};
    lines.size = 3;
    
    // @Note: This is a placeholder for now, just to start
    // with some basic points.
    {
        lines.data[0].x0 = RECT_ROWS/2;
        lines.data[0].y0 = 10;
        lines.data[1].x0 = RECT_ROWS/8;
        lines.data[1].y0 = 20;
        lines.data[2].x0 = RECT_ROWS - 10;
        lines.data[2].y0 = 30;
    
        lines.data[0].x1 = lines.data[1].x0;
        lines.data[0].y1 = lines.data[1].y0;
        lines.data[1].x1 = lines.data[2].x0;
        lines.data[1].y1 = lines.data[2].y0;
        lines.data[2].x1 = lines.data[0].x0;
        lines.data[2].y1 = lines.data[0].y0;
    }

    // @Note: Create initial board.
    for (u32 row = 0; row < RECT_ROWS; ++row) {
        for (u32 col = 0; col < RECT_COLS; ++col) {
            SDL_Rect rect = {0};
            rect.w = rect.h = RECT_RES;                        
            rect.x = row * rect.w;
            rect.y = col * rect.h;
            ARRAY_AT(rects, row, col) = rect;
        }
    }
    
    rasterize_shape(&lines, rects, filled_rects);
    
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
                        line_index = get_index_of_selected_origin(e.button.x, e.button.y, &lines);
                    } else if (e.button.button == SDL_BUTTON_RIGHT) {
                        printf("ToDo: Pressed RMB\n");
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
                        f32 x = FLOORF(((f32) e.motion.x/WIDTH) * RECT_ROWS);
                        f32 y = FLOORF(((f32) e.motion.y/HEIGHT) * RECT_COLS);
                        
                        if ((x > 0.0f && x < RECT_ROWS) && (y > 0.0f && y < RECT_COLS)) {
                            // @Note: Simple way to loop back when line_index = 0.
                            s32 connected_line_index = line_index - 1 == -1 ? (s32) lines.size - 1 : line_index - 1;
                            
                            lines.data[line_index].x0 = x;
                            lines.data[line_index].y0 = y;
                            lines.data[connected_line_index].x1 = x;
                            lines.data[connected_line_index].y1 = y;
                        
                            rasterize_shape(&lines, rects, filled_rects);
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

        for (u32 i = 0; i < lines.size; ++i) {
            u32 x1 = (u32) (lines.data[i].x0 * RECT_RES);
            u32 y1 = (u32) (lines.data[i].y0 * RECT_RES);
            u32 x2 = (u32) (lines.data[i].x1 * RECT_RES);
            u32 y2 = (u32) (lines.data[i].y1 * RECT_RES);

            SDL_SetRenderDrawColor(context.renderer, 255, 0, 0, 255);
            SDL_RenderDrawLine(context.renderer, x1, y1, x2, y2);

            render_draw_circle(context.renderer, x1, y1, CIRCLE_RADIUS, 255, 0, 0);
        }

        SDL_RenderPresent(context.renderer);
        if (time_to_wait > 0 && time_to_wait < MS_PER_FRAME) SDL_Delay(time_to_wait);
    }

    destroy_render_context(&context);

    return 0;
}
