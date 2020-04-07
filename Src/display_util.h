#pragma once
#include <stdint.h>
#include "font.h"

/* A panel is a 16x16 section of LEDs */
/* We have two displays, consisting of 4 panels *each* */
#define NUM_PANELS  4

#define FRAMEBUFFER_SIZE    (256 * NUM_PANELS)

#define DISPLAY_WIDTH       (16 * NUM_PANELS)
#define DISPLAY_HEIGHT      16

/* The order of R, G, B is important, because that's the order these get clocked out.
 * The LEDs expect the green channel first, then red, then blue.
 */
struct pixel {
    uint8_t g;
    uint8_t r;
    uint8_t b;
};

/* Vertical scroll effect */
struct odometer_state {
    char old;
    char new;
    int pos;
};

void odometer_init(struct odometer_state *state);
void odometer_scroll_on(char new, struct odometer_state *state);
int draw_odometer_effect(int x, int y, struct odometer_state *state, const struct font* f, int color, struct pixel *fb);
int draw_text(const char *txt, int32_t skip, const struct font *f, struct pixel *fb);
