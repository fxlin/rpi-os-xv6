#ifndef INC_CONSOLE_H
#define INC_CONSOLE_H

#include <stdarg.h>

extern struct spinlock conslock;

void console_init();
void console_intr(int (*getc)());
void cgetchar(int c);
void cprintf(const char *fmt, ...);
void panic(const char *fmt, ...);

#define assert(x)                                                   \
({                                                                  \
    if (!(x)) {                                                     \
        panic("%s:%d: assertion failed.\n", __FILE__, __LINE__);    \
    }                                                               \
})

#define asserts(x, ...)                                             \
({                                                                  \
    if (!(x)) {                                                     \
        cprintf("%s:%d: assertion failed.\n", __FILE__, __LINE__);  \
        panic(__VA_ARGS__);                                         \
    }                                                               \
})

#define LOG1(level, ...)                    \
({                                          \
    acquire(&conslock);                     \
    cprintf1("%s: ", __func__);   \
    cprintf1(__VA_ARGS__);                  \
    cprintf1("\n");                         \
    release(&conslock);                     \
})

#ifdef LOG_ERROR
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)
#define info(...)
#define debug(...)
#define trace(...)

#elif defined(LOG_WARN)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)
#define debug(...)
#define trace(...)

#elif defined(LOG_INFO)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)
#define trace(...)

#elif defined(LOG_DEBUG)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)  LOG1("DEBUG", __VA_ARGS__);
#define trace(...)

#elif defined(LOG_TRACE)
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)   LOG1("INFO ", __VA_ARGS__);
#define debug(...)  LOG1("DEBUG", __VA_ARGS__);
#define trace(...)  LOG1("TRACE", __VA_ARGS__);

#else
/* Default to LOG_WARN */
#define error(...)  LOG1("ERROR", __VA_ARGS__);
#define warn(...)   LOG1("WARN ", __VA_ARGS__);
#define info(...)
#define debug(...)
#define trace(...)

#endif

#endif
