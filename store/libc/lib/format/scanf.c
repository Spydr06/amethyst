#include <stdio.h>

#include <unistd.h>
#include <string.h>
#include <sys/param.h>

#include <internal/format.h>

struct str_data {
    const char *restrict src;
    size_t src_size;
    size_t src_offset;
};

// Macro to expand variadic args
#define V(name, format, ...) do {               \
        va_list __ap;                           \
        va_start(__ap, format);                 \
        int __r = v##name(__VA_ARGS__, __ap);   \
        va_end(__ap);                           \
        return __r;                             \
    } while(0)

static ssize_t stream_scanner(void *restrict userp, char *restrict dst, size_t size) {
    FILE *restrict stream = userp;
    return fread(dst, size, sizeof(char), stream);
}

static ssize_t string_scanner(void *restrict userp, char *restrict str, size_t size) {
    struct str_data *restrict data = userp;

    if(data->src_offset >= data->src_size)
        return EOF;

    size_t actual_size = MIN(data->src_size - data->src_offset, size);
    
    memcpy(str, data->src + data->src_offset, actual_size);

    data->src_offset += actual_size;
    return actual_size;
}

int scanf(const char *restrict format, ...) {
    V(scanf, format, format);
}

int vscanf(const char *restrict format, va_list ap) {
    return vfscanf(stdin, format, ap);
}

int fscanf(FILE *restrict stream, const char *restrict format, ...) {\
    V(fscanf, format, stream, format);
}

int vfscanf(FILE *restrict stream, const char *restrict format, va_list ap) {
    return scanf_impl(stream_scanner, stream, format, ap);
}

int sscanf(const char *restrict str, const char *restrict format, ...) {
    V(sscanf, format, str, format);
}

int vsscanf(const char *restrict str, const char *restrict format, va_list ap) {
    struct str_data data = {
        .src = str,
        .src_offset = 0,
        .src_size = strlen(str)
    };
    return scanf_impl(string_scanner, &data, format, ap);
}

