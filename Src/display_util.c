#include "display_util.h"
#include "font.h"

unsigned int map_coords(unsigned int x, unsigned int y)
{
    int idx = x * 16;

    if (x & 1)
        return idx + 15 - y;
    else
        return idx + y;
}

void set_pixel(struct pixel* framebuffer, int index, uint32_t color)
{
    int scale = 1;
    framebuffer[index].b = (color & 0xff) / scale;
    framebuffer[index].g = (color >> 8 & 0xff) / scale;
    framebuffer[index].r = (color >> 16 & 0xff) / scale;
}

void draw_var_char(char c, int x, int y, int start_line, int num_lines, int shift, const struct font *f, uint32_t color, struct pixel *fb)
{
    int i, j;

    if (num_lines + start_line > f->h)
        num_lines = f->h - start_line;

    if (num_lines <= 0)
        return;

    if (num_lines + y > DISPLAY_HEIGHT)
        num_lines = DISPLAY_HEIGHT - y;

    for (i = 0; i < num_lines; i++) {

        uint16_t cur_line = f->data[ (c - ' ') * f->h + i + start_line] << shift;

        for (j = 0; j < f->w; j++) {

            if (cur_line & ((1 << (f->w - j - 1)))) {
                if ((x + j) >= 0 && (y + i) >= 0 && (x + j) < DISPLAY_WIDTH && (y + i) < DISPLAY_HEIGHT)
                    set_pixel(fb, map_coords(x + j, i + y), color);
            }
        }
    }
}

void draw_char(char c, int x, int y, int start_line, int num_lines, const struct font *f, uint32_t color, struct pixel *fb)
{
    draw_var_char(c, x, y, start_line, num_lines, 0, f, color, fb);
}

int draw_odometer_effect(int x, int y, struct odometer_state *state, const struct font* f, int color, struct pixel *fb)
{
    int height = f->h;
    int pos = state->pos;

    int old_color = color;
    int new_color = color;

    /* We're done; just draw 'new' */
    if (state->pos == -1) {
        draw_char(state->new, x, y, 0, height, f, new_color, fb);
        return 1;
    }

    draw_char(state->new, x, y, height - pos, height, f, new_color, fb);
    draw_char(state->old, x, y + pos, 0, height, f, old_color, fb);

    state->pos++;

    if (state->pos >= height) {
        state->old = state->new;
        state->pos = -1;
        return 1;
    }

    return 0;
}

void odometer_scroll_on(char new, struct odometer_state *state) {
    if (state->old == new || state->pos != -1)
        return;

    state->old = state->new;
    state->new = new;
    state->pos = 0;
}

void odometer_init(struct odometer_state *state) {
    state->old = ' ';
    state->new = ' ';
    state->pos = -1;
}

uint32_t text_colors[] = {
    0xff0000,
    0xff4000,
    0x00ff00,
    0x00ffff,
    0xffffff,
};

int draw_text(const char *txt, int32_t skip, const struct font *f, struct pixel *fb)
{
    int32_t pos = 0;
    int idx = 0;
    int spacing = 3;
    char cur;
    int cur_width, shift;
    int drawn = 0;
    uint32_t color = 0x00ff00;
    int start_x = 0;

    if (!txt)
        return 0;

    /* Scroll on a new line from the right edge of the panel */
    if (skip < DISPLAY_WIDTH) {
        start_x = DISPLAY_WIDTH - skip;
        skip = 0;
    } else {
        skip -= DISPLAY_WIDTH;
    }

    while (pos < DISPLAY_WIDTH && txt[idx] != 0) {
        cur = txt[idx];

        /* Escape sequence for color changes? */
        if (cur == '\\') {
            color = text_colors[txt[idx + 1] - '0'];
            idx += 2;
            continue;
        }

        cur_width = char_width(cur, f) + spacing;

        if (skip >= cur_width) {
            skip -= cur_width;
            idx++;
            continue;
        }

        if (skip) {
            pos = -skip;
            skip = 0;
        }

        shift = f->w - char_width(cur, f) - 1;
        draw_var_char(cur, pos + start_x, -1, 0, f->h, shift, f, color, fb);
        drawn++;
        pos += cur_width;
        idx++;
    }

    return drawn;
}
