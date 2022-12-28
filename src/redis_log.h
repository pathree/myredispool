#ifndef REDIS_LOG_H_
#define REDIS_LOG_H_

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace myredis {

enum LOG_LEVEL {
  LOG_EMERG = 0,
  LOG_ALERT = 1,
  LOG_CRIT = 2,
  LOG_ERR = 3,
  LOG_WARNING = 4,
  LOG_NOTICE = 5,
  LOG_INFO = 6,
  LOG_DEBUG = 7
};

const static char* lv_str[] = {"EME", "ALE", "CRIT", "ERR",
                               "WAR", "NOT", "INF",  "DBG"};

#define x_print(lv, fmt, ...)                                            \
  do {                                                                   \
    if (lv > LOG_INFO) break;                                            \
    struct timeval tv;                                                   \
    struct tm t;                                                         \
    gettimeofday(&tv, NULL);                                             \
    localtime_r(&tv.tv_sec, &t);                                         \
    char buf[1024] = {0};                                                \
    int n = snprintf(buf, 1023, "[%02d:%02d:%02d.%06d] [%s] [%ld] " fmt, \
                     t.tm_hour, t.tm_min, t.tm_sec, (int)tv.tv_usec,     \
                     lv_str[lv], syscall(SYS_gettid), ##__VA_ARGS__);    \
    if (buf[n - 1] != '\n') buf[n] = '\n';                               \
    printf(buf);                                                         \
  } while (0)

#define x_error(fmt, ...) x_print(LOG_ERR, fmt, ##__VA_ARGS__)
#define x_notice(fmt, ...) x_print(LOG_NOTICE, fmt, ##__VA_ARGS__)
#define x_info(fmt, ...) x_print(LOG_INFO, fmt, ##__VA_ARGS__)
#define x_debug(fmt, ...) x_print(LOG_DEBUG, fmt, ##__VA_ARGS__)

}  // namespace myredis

#endif  // REDIS_LOG_H_