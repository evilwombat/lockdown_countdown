/*
 * Copyright (c) 2020, evilwombat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ws2812_led.h"
#include "font.h"
#include "display_util.h"
#include "util.h"

extern RTC_HandleTypeDef hrtc;
extern int BUILD_TIME;

struct pixel framebuffer_top[FRAMEBUFFER_SIZE];
struct pixel framebuffer_bottom[FRAMEBUFFER_SIZE];

#define TXT_RED         "\\0"
#define TXT_YELLOW      "\\1"
#define TXT_GREEN       "\\2"
#define TXT_BLUE        "\\3"
#define TXT_WHITE       "\\4"

const int rainbow_colors[] = {
    0xFF0000,
    0xFF3000,
    0xAF6F00,
    0x00D700,
    0x008383,
//    0x1010FF, /* Blue is hard to see from afar */
    0x7f007f,
    0x4f4f4f,
    0x00FF40,
    0x60FF1F,
    0xFFFFFF,
};

const int night_colors[] = {
    0xFF0000,
    0xFF4000,
    0x9F8F00,
    0x00DF00,
    0x009090,
//    0x1010FF, /* Blue is hard to see from afar */
    0x800080,
    0x4f4f4f,
    0x00FF40,
    0x60FF1F,
    0xFFFFFF,
};

#define NUM_SCROLL_DIGITS   7
struct odometer_state digits[NUM_SCROLL_DIGITS];

int get_seconds_remaining(int32_t time_now)
{
    int32_t end_sec;
    struct tm end_time;
    memset(&end_time, 0, sizeof(end_time));

    end_time.tm_mday = 3;
    end_time.tm_mon = 4;    // May (they start from 0)
    end_time.tm_year = 2020 - 1900;

    end_sec = mktime(&end_time);

    return end_sec - time_now;
}

void scale_framebuffer(struct pixel *fb, int len, int scale) {
    int i;
    for (i  = 0; i < len; i++) {
        fb[i].r /= scale;
        fb[i].g /= scale;
        fb[i].b /= scale;
    }
}

void generate_time_left_msg(char *msg, int max_len, int time_left)
{
    int days_left = time_left / (24 * 60 * 60);
    int hours_left = time_left / (60 * 60) % 24;
    int min_left = (time_left / 60) % 60;
    int sec_left = time_left % 60;

    if (time_left > 0) {
        snprintf(msg, max_len,
            TXT_GREEN "Lockdown ends in "
            TXT_RED "%d " TXT_GREEN "Day%s, "
            TXT_RED "%d " TXT_GREEN "Hour%s, "
            TXT_RED "%d " TXT_GREEN "Minute%s, "
            TXT_RED "%d " TXT_GREEN "Second%s",
            days_left, days_left == 1 ? "" : "s",
            hours_left, hours_left == 1 ? "" : "s",
            min_left, min_left == 1 ? "" : "s",
            sec_left, sec_left == 1 ? "" : "s"
        );
    } else {
        snprintf(msg, max_len, "\\0The lockdown is over??? What does the news say?");
    }
}

const char *static_messages[] = {
    TXT_BLUE    "Wombats are marsupials native to Australia.",
    TXT_GREEN   "Hello world",
    TXT_YELLOW  "Sanitizer?  I barely even know her!",
    TXT_BLUE    "Wash your hands, guys",
    TXT_WHITE   "Did you call your family today?",
    TXT_YELLOW  "Social distancing saves lives",
    TXT_WHITE   "No, the airlines do " TXT_RED "not" TXT_WHITE " need a bailout.",
    TXT_GREEN   "github.com/" TXT_YELLOW "evilwombat",
    TXT_YELLOW  "Video-call your friends on the other coast!",
    TXT_BLUE    "Now's a good time to catch up on video games!",
    TXT_GREEN   "Coming soon: " TXT_YELLOW "50 Recipes for Cooking Toilet Paper",
    TXT_BLUE    "Most Australians have never seen a wild wombat.",
    TXT_GREEN   "No, Wall Street does " TXT_RED "not" TXT_GREEN " need a bailout, either.",
    TXT_WHITE   "What's the first thing you're gonna do when this is over?",
    TXT_RED     "At least air pollution is way down.",
    NULL
};

int app_main(void)
{
    int32_t time_now;
    int time_left;
    int last_time_left = -1;
    int night_mode = 0;
    int scale = 64;
    int scroll_cooldown = 0;
    struct tm *tm_now;

    char countdown_text[NUM_SCROLL_DIGITS + 1];
    char time_left_msg[1024];   /* Ghoulish overkill? */

    struct led_channel_info led_channels[WS2812_NUM_CHANNELS];
    int i;

    struct {
        int countdown_mode;
        int static_msg;
        int scroll_pos;
        const char *cur_msg;
    } hscroll_state;

    hscroll_state.countdown_mode = 0;
    hscroll_state.static_msg = 0;
    hscroll_state.scroll_pos = 0;
    hscroll_state.cur_msg = "";

    /* Only set the RTC if it isn't already set on target. That way, if the target reboots, it
     * doesn't revert back to the last time the code was built.
     */
    if (RTC_ReadTimeCounter(&hrtc) < 1000) {

        /* HACK: We represent BUILD_TIME in UTC, but want our target to run in Pacific time
         * We add two seconds because that's approximately how long it takes to flash our target
         */
        RTC_WriteTimeCounter(&hrtc, BUILD_TIME - 7 * 3600 + 2);
    }

    __enable_irq();

    for (int i = 0; i < WS2812_NUM_CHANNELS; i++) {
        led_channels[i].framebuffer = NULL;
        led_channels[i].length = 0;
    }

    /* Set up the framebuffer pointers for LED refresh
     * Our system consists of two displays, each being 4 panels of 256 LEDs.
     * Therefore, we have two framebuffers - one for the top display and one for the bottom.
     * But, even though the 16x16 panels are physically arranged in two groups of four, they
     * are still refreshed INDIVIDUALLY, to save time. Therefore, we actually refresh eight
     * channels of LEDs at a time. This means each 16x16 panel has a dedicated control wire
     * running to it.
     * 
     * Despite this, we only have two framebuffers. So, each group of 16x16 panels may be
     * refreshed from the same framebuffer, just from a different offset.
     * */

    /* Top display - this shows scrolling text messages */
    led_channels[0].framebuffer = (uint8_t*) (framebuffer_top + 0);
    led_channels[0].length = sizeof(struct pixel) * 256;

    led_channels[1].framebuffer = (uint8_t*) (framebuffer_top + 256);
    led_channels[1].length = sizeof(struct pixel) * 256;

    led_channels[2].framebuffer = (uint8_t*) (framebuffer_top + 512);
    led_channels[2].length = sizeof(struct pixel) * 256;

    led_channels[3].framebuffer = (uint8_t*) (framebuffer_top + 768);
    led_channels[3].length = sizeof(struct pixel) * 256;


    /* Bottom display - this shows odometer digits */
    led_channels[4].framebuffer = (uint8_t*) (framebuffer_bottom + 0);
    led_channels[4].length = sizeof(struct pixel) * 256;

    led_channels[5].framebuffer = (uint8_t*) (framebuffer_bottom + 256);
    led_channels[5].length = sizeof(struct pixel) * 256;

    led_channels[6].framebuffer = (uint8_t*) (framebuffer_bottom + 512);
    led_channels[6].length = sizeof(struct pixel) * 256;

    led_channels[7].framebuffer = (uint8_t*) (framebuffer_bottom + 768);
    led_channels[7].length = sizeof(struct pixel) * 256;

    ws2812_init();

    /* Set up state machine for odometer animation */
    for (i = 0; i < NUM_SCROLL_DIGITS; i++)
        odometer_init(digits + i);

    while(1) {
        memset(framebuffer_top, 0, sizeof(framebuffer_top));

        time_now = RTC_ReadTimeCounter(&hrtc);
        time_left = get_seconds_remaining(time_now);

        tm_now = gmtime(&time_now);

        /* Set the LED brightness based on time of day. */
        if (tm_now) {
            if (tm_now->tm_hour > 19 || tm_now->tm_hour < 8)
                night_mode = 1;
            else
                night_mode = 0;
        }

        if (time_left < 0)
            time_left = 0;

        if (time_left != last_time_left) {
            snprintf(countdown_text, sizeof(countdown_text), "%7d", time_left);

            for (i = 0; i < NUM_SCROLL_DIGITS; i++)
                odometer_scroll_on(countdown_text[i], digits + i);

            /* If the time changes, reset the odometer cooldown immediately. This prevents stutter */
            scroll_cooldown = 0;
        }

        if (night_mode)
            scale = 64;             /* Night mode */
        else
            scale = 8;              /* Day mode */

        last_time_left = time_left;

        /* We don't want the odometer to animate on every frame; it's too fast. So, implement a cooldown */
        if (scroll_cooldown == 0) {
            memset(framebuffer_bottom, 0, sizeof(framebuffer_bottom));

            /* This also advances the odometer animation by one step */
            for (i = 0; i < NUM_SCROLL_DIGITS; i++)
                draw_odometer_effect(i * 9, 0, digits + i, &font_9_14,
                                     night_mode ? night_colors[i] : rainbow_colors[i], framebuffer_bottom);

            /* Adjust the brightness on the odometer based on current scale */
            scale_framebuffer(framebuffer_bottom, FRAMEBUFFER_SIZE, scale);

            /* and restart the cooldown.  */
            scroll_cooldown = 1;
        } else {
            scroll_cooldown--;
        }

        /* Now, deal with the horizontally scrolling text */
        int ret = draw_text(hscroll_state.cur_msg, hscroll_state.scroll_pos, &font_9_14, framebuffer_top);
        hscroll_state.scroll_pos++;

        /* We want the horizontal text to alternate between a "dynamic" message containing the
         * time to end of lockdown, and "static" messages from a table.
         */

        if (ret == 0) {   /* Are we done scrolling the current message? */
            hscroll_state.scroll_pos = 0;

            /* Have we finished scrolling a static message? Prepare the countdown message */ 
            /* Skip the static messages if we think the lockdown is over */
            if (!hscroll_state.countdown_mode || time_left == 0) {
                hscroll_state.countdown_mode = 1;
                hscroll_state.cur_msg = time_left_msg;
                generate_time_left_msg(time_left_msg, sizeof(time_left_msg), time_left);
            } else {                
                hscroll_state.countdown_mode = 0;

                /* Pick a new message from the message table */
                hscroll_state.cur_msg = static_messages[hscroll_state.static_msg];
                hscroll_state.static_msg++;

                /* Have we reached the end of the table? Go back to the beginning */
                if (static_messages[hscroll_state.static_msg] == NULL)
                    hscroll_state.static_msg = 0;
            }
        }

        scale_framebuffer(framebuffer_top, FRAMEBUFFER_SIZE, scale);

        HAL_Delay(1);

        __disable_irq();
        ws2812_refresh(led_channels, GPIOB);
        __enable_irq();
    }

    while(1);
}
