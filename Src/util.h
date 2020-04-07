void uart_printf(const char *fmt, ...);

#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#define ABS(a)  (((a) > 0) ? (a) : (-(a)))

#define STEP_UP(v, max)    { (v)++; (v) = (v) < max ? (v) : ((v) - (max)); }
#define STEP_DOWN(v, max)  { (v) = (v) == 0 ? (max) - 1 : (v) - 1; }

#define STEP_VALUE(v, delta, max) {  \
    int tmp = (v); \
    tmp += (delta); \
    if ((tmp) < 0) (tmp) += (max); \
    if ((tmp) >= (max)) (tmp) -= (max); \
    (v) = tmp; \
    }

#define ARRAY_SIZE(a)  (sizeof(a) / (sizeof((a)[0])))

int step_value(int old_value, int delta, int max);
int get_time();
void adjust_hours(int delta);
void adjust_minutes(int delta);
int get_time_seconds();
int get_time_minutes();
