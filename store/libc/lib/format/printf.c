#include <stdio.h>

#include <unistd.h>
#include <string.h>
#include <sys/param.h>

#include <internal/format.h>

// Macro to expand variadic args
#define V(name, format, ...) do {               \
        va_list __ap;                           \
        va_start(__ap, format);                 \
        int __r = v##name(__VA_ARGS__, __ap);   \
        va_end(__ap);                           \
        return __r;                             \
    } while(0)

struct strn_data {
    char *restrict dst;
    size_t dst_size;
    size_t dst_offset;
};

struct str_data {
    char *restrict dst;
    size_t dst_offset;
};

static ssize_t stream_printer(void *restrict userp, const char *restrict str, size_t size) {
    FILE *restrict stream = userp;
    return fwrite(str, sizeof(char), size, stream);
}

static ssize_t fd_printer(void *restrict userp, const char *restrict str, size_t size) {
    int fd = *(int *restrict) userp;
    return write(fd, str, size * sizeof(char));
}

static ssize_t str_printer(void *restrict userp, const char *restrict str, size_t size) {
    struct str_data *restrict data = userp;
    memcpy(data->dst + data->dst_offset, str, size * sizeof(char));
    data->dst_offset += size;
    data->dst[data->dst_offset] = '\0';
    return size;
}

static ssize_t strn_printer(void *restrict userp, const char *restrict str, size_t size) {
    struct strn_data *restrict data = userp;
    size_t actual_size = MIN(data->dst_size - data->dst_offset, size);

    memcpy(data->dst + data->dst_offset, str, actual_size);
    data->dst_offset += actual_size;

    if(data->dst_offset >= data->dst_size)
        data->dst[data->dst_size - 1] = '\0';
    else
        data->dst[data->dst_offset] = '\0';

    return actual_size;
}

int printf(const char *restrict format, ...) {
    V(printf, format, format);
}

int vprintf(const char *restrict format, va_list ap) {
    return vfprintf(stdout, format, ap);
}

int fprintf(FILE *restrict stream, const char *restrict format, ...) {
    V(fprintf, format, stream, format);
}

int vfprintf(FILE *restrict stream, const char *restrict format, va_list ap) {
    return printf_impl(stream_printer, stream, format, ap);
}

int dprintf(int fd, const char *restrict format, ...) {
    V(dprintf, format, fd, format);
}

int vdprintf(int fd, const char *restrict format, va_list ap) {
    return printf_impl(fd_printer, &fd, format, ap);
}

int sprintf(char *restrict str, const char *restrict format, ...) {
    V(sprintf, format, str, format);
}

int vsprintf(char *restrict str, const char *restrict format, va_list ap) {
    struct str_data data = {.dst = str, .dst_offset = 0};
    return printf_impl(str_printer, &data, format, ap);
}

int snprintf(char *restrict str, size_t size, const char *restrict format, ...) {
    V(snprintf, format, str, size, format);
}

int vsnprintf(char *restrict str, size_t size, const char *restrict format, va_list ap) {
    struct strn_data data = {.dst = str, .dst_size = size, .dst_offset = 0};
    return printf_impl(strn_printer, &data, format, ap);
}

