#pragma once
#include <stdint.h>

struct font {
    int w, h;
    const uint16_t *data;
};

int char_width(char c, const struct font *f);
extern const struct font font_12_16;
extern const struct font font_9_14;
