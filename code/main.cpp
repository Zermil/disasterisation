#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>

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
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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

struct Vec2f {
    f32 x;
    f32 y;
};

struct Line {
    u32 x0;
    u32 y0;
    u32 x1;
    u32 y1;

    // @Note: Connections are supposed to _always_ be clockwise.
    // We are working under that assumption in most of the code.
    size_t next;
    size_t prev;
};

struct Line_Array {
    Line data[LINES_MAX];
    size_t size;
};

// @ToDo: This could be in some struct.
global bool mouse_held = false;
global s32 line_index = -1;

internal inline void line_array_add(Line_Array *lines, s32 x0, s32 y0, s32 x1, s32 y1)
{
    if (lines->size == LINES_MAX) return;
   
    lines->data[lines->size].x0 = x0;
    lines->data[lines->size].y0 = y0;
    lines->data[lines->size].x1 = x1;
    lines->data[lines->size].y1 = y1;
    lines->size += 1;
}

internal inline void line_array_connect(Line_Array *lines, size_t which, size_t next, size_t prev)
{
    assert(which < lines->size);
    assert(next < lines->size);
    assert(prev < lines->size);

    lines->data[which].next = next;
    lines->data[which].prev = prev;
}

// @ToDo: Would be cool to get rid off floating point math here, not super important
// but just something to think about.
internal bool check_intersection(Line line, Vec2f Bs, Vec2f Bd, f32 *t, f32 *u)
{
    Vec2f As = {(f32) line.x0, (f32) line.y0};
    Vec2f Ad = {line.x1 - As.x, line.y1 - As.y};

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

// @ToDo: Add more fill rules to see how they work on different shapes.
internal void rasterize_shape(Line_Array *lines, SDL_Rect *rects, SDL_Rect *filled_rects)
{
    memset(filled_rects, 0, sizeof(SDL_Rect)*RECT_ROWS*RECT_COLS);
    
    u32 min_x = lines->data[0].x0;
    u32 max_x = lines->data[0].x0;
    u32 min_y = lines->data[0].y0;
    u32 max_y = lines->data[0].y0;

    // @Note: Lines are all connected no need to check
    // lines->data[i].x1/y1 as it will just be the next item.
    for (size_t i = 1; i < lines->size; ++i) {
        min_x = MIN(lines->data[i].x0, min_x);
        max_x = MAX(lines->data[i].x0, max_x);

        min_y = MIN(lines->data[i].y0, min_y);
        max_y = MAX(lines->data[i].y0, max_y);
    }
    
    f32 t, u;    
    for (u32 row = min_x; row < max_x; ++row) {
        for (u32 col = min_y; col < max_y; ++col) { 
            u32 intersections = 0;
            for (size_t i = 0; i < lines->size; ++i) {
                if (!check_intersection(lines->data[i], {row + 0.5f, col + 0.5f}, {-1.0f, 0.0f}, &t, &u)) continue;
                
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
        s32 x = (s32) lines->data[i].x0 * RECT_RES - CIRCLE_RADIUS;
        s32 y = (s32) lines->data[i].y0 * RECT_RES - CIRCLE_RADIUS;
        
        if ((mouse_x >= x && mouse_x <= x + w) &&
            (mouse_y >= y && mouse_y <= y + w))
        {
            return(i);
        }
    }

    return(-1);
}

// @ToDo: Points like (x0 = 40, y0 = 29) cause some weird trouble.
internal void add_new_point(s32 mouse_x, s32 mouse_y, Line_Array *lines)
{
    assert(lines->size > 0);

    u32 x0 = (u32) ((f32) mouse_x/WIDTH * RECT_ROWS);
    u32 y0 = (u32) ((f32) mouse_y/HEIGHT * RECT_COLS);
    u32 x1 = lines->data[0].x0;
    u32 y1 = lines->data[0].y0;
    
    u32 min_dist = (x1 - x0)*(x1 - x0) + (y1 - y0)*(y1 - y0);
    size_t index = 0;
    
    // @Note: Find the closes point to our newly created one.
    // Since they are all connected, we just need to check x0/y0
    // x1/y1 will just be the next item in array.
    for (size_t i = 1; i < lines->size; ++i) {
        u32 dx = lines->data[i].x0 - x0;
        u32 dy = lines->data[i].y0 - y0;
        u32 dist = dx*dx + dy*dy;

        if (dist < min_dist) {
            index = i;
            min_dist = dist;
        }
    }

    // @Note: Check if line intersects with any other line (excluding itself)
    // in order to find something that nicely and seemlesly connects.
    size_t con_index = lines->data[index].next;
    Vec2f Bs = {(f32) x0, (f32) y0};
    Vec2f Bd = {(f32) lines->data[con_index].x0 - x0, (f32) lines->data[con_index].y0 - y0};
    f32 t, u;

    for (size_t i = 0; i < lines->size; ++i) {
        if (!check_intersection(lines->data[i], Bs, Bd, &t, &u)) continue;

        if (u >= 0.0f && (t >= 0.0f && t <= 1.0f)) {            
            if (Bs.x + Bd.x*u != lines->data[con_index].x0 && Bs.x + Bd.x*u != lines->data[con_index].y0) {
                con_index = lines->data[index].prev;
                break;
            }
        }
    }

    // @ToDo: Refactor this, too many low-level operations. These
    // are very self-similar so Semantic-Compression should be applied (common pattern).
    if (con_index == lines->data[index].next) {        
        line_array_add(lines, x0, y0, lines->data[con_index].x0, lines->data[con_index].y0);
        line_array_connect(lines, lines->size - 1, con_index, index);
        
        lines->data[con_index].prev = lines->size - 1;
        lines->data[index].next = lines->size - 1;
        lines->data[index].x1 = x0;
        lines->data[index].y1 = y0;
    } else {
        line_array_add(lines, x0, y0, lines->data[index].x0, lines->data[index].y0);
        line_array_connect(lines, lines->size - 1, index, con_index);

        lines->data[con_index].next = lines->size - 1;
        lines->data[index].prev = lines->size - 1;
        lines->data[con_index].x1 = x0;
        lines->data[con_index].y1 = y0;
    }
}

internal void render_draw_circle(SDL_Renderer *renderer, u32 cx, u32 cy, u32 r)
{
    u32 x = r;
    u32 y = 0;
    s32 p = 1 - r;
    
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

    // @Note: This is a placeholder for now, just to start
    // with some basic points.
    {
        line_array_add(&lines, RECT_ROWS/8, 20, RECT_ROWS/2, 10);
        line_array_add(&lines, RECT_ROWS/2, 10, RECT_ROWS - 10, 30);
        line_array_add(&lines, RECT_ROWS - 10, 30, RECT_ROWS/8, 20);

        line_array_connect(&lines, 0, 1, 2);
        line_array_connect(&lines, 1, 2, 0);
        line_array_connect(&lines, 2, 0, 1);
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
                        add_new_point(e.button.x, e.button.y, &lines);
                        rasterize_shape(&lines, rects, filled_rects);
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
                        u32 x = (u32) (((f32) e.motion.x/WIDTH) * RECT_ROWS);
                        u32 y = (u32) (((f32) e.motion.y/HEIGHT) * RECT_COLS);
                        
                        if ((x > 0.0f && x < RECT_ROWS) && (y > 0.0f && y < RECT_COLS)) {
                            size_t connected_line = lines.data[line_index].prev;
                            lines.data[line_index].x0 = lines.data[connected_line].x1 = x;
                            lines.data[line_index].y0 = lines.data[connected_line].y1 = y;
                        
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
            u32 x0 = lines.data[i].x0 * RECT_RES;
            u32 y0 = lines.data[i].y0 * RECT_RES;
            u32 x1 = lines.data[i].x1 * RECT_RES;
            u32 y1 = lines.data[i].y1 * RECT_RES;

            SDL_SetRenderDrawColor(context.renderer, 255, 0, 0, 255);
            
            SDL_RenderDrawLine(context.renderer, x0, y0, x1, y1);
            render_draw_circle(context.renderer, x0, y0, CIRCLE_RADIUS);
        }

        SDL_RenderPresent(context.renderer);
        if (time_to_wait > 0 && time_to_wait < MS_PER_FRAME) SDL_Delay(time_to_wait);
    }

    destroy_render_context(&context);

    return 0;
}
