#ifndef LOGGER_H_
#define LOGGER_H_

#define COLOR_ERROR   "\033[38;5;160m"
#define COLOR_BUG     "\033[38;5;204m"
#define COLOR_WARNING "\033[38;5;3m"
#define COLOR_INFO    "\033[38;5;12m"
#define COLOR_RESET   "\033[0m"

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "%s[ERROR]: " fmt "%s\n", COLOR_ERROR, ##__VA_ARGS__, COLOR_RESET)
#define LOG_BUG(fmt, ...) \
    fprintf(stderr, "%s[BUG]: " fmt "%s\n", COLOR_BUG, ##__VA_ARGS__, COLOR_RESET)
#define LOG_INFO(fmt, ...) \
    fprintf(stderr, "%s[INFO]: " fmt "%s\n", COLOR_INFO, ##__VA_ARGS__, COLOR_RESET)
#define LOG_WARNING(fmt, ...) \
    fprintf(stderr, "%s[WARNING]: " fmt "%s\n", COLOR_WARNING, ##__VA_ARGS__, COLOR_RESET)


#endif
