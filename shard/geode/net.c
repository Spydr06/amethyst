#include "net.h"
#include "geode.h"
#include "exception.h"
#include "include/log.h"
#include "util.h"

#include <curl/curl.h>
#include <curl/easy.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

static CURL *curl;

#define PROGRESS_BAR_MIN_WIDTH 20

struct download_context {
    struct geode_context *context;

    const char *filename;

    curl_off_t dl_last;
    time_t t_begin;
    time_t t_last;
};

void net_lazy_init(struct geode_context *) {
    if(!curl)
        curl = curl_easy_init();
    if(!curl)
        assert(!"curl init failed");
}

static void format_bytecount(char *buf, size_t buf_size, uint64_t byte_count) {
    static const char *units[] = { "B", "KiB", "MiB", "GiB", "TiB" };
    uint8_t unit = 0;

    for(unit = 0; byte_count > 1024 * 10 && unit < 4; byte_count /= 1024, unit++);

    snprintf(buf, buf_size, "% 4ld.%1ld %s", byte_count, ((byte_count * 10) % 10240) / 1024, units[unit]);
}

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
    FILE *fp = (FILE*) stream;
    return fwrite(ptr, size, nmemb, fp);
}

static void xfer_callback(void *userp, curl_off_t dl_total, curl_off_t dl_now, curl_off_t ul_total, curl_off_t ul_now) {
    (void) ul_total;
    (void) ul_now;

    if(dl_total == 0)
        return;

    struct download_context *dl = (struct download_context*) userp;

    time_t t_now = time(NULL); 
    time_t t_delta = MAX(t_now - dl->t_last, 1);
    time_t t_since = dl->t_last - dl->t_begin;

    curl_off_t dl_delta = dl_now - dl->dl_last;
    curl_off_t dl_rate = dl_delta / t_delta;
    curl_off_t dl_promille = dl_now ? dl_total * 1000 / dl_now : 0;

    char dl_total_buf[32], dl_now_buf[32], dl_rate_buf[32];
    format_bytecount(dl_total_buf, sizeof(dl_total_buf), dl_total);
    format_bytecount(dl_now_buf, sizeof(dl_now_buf), dl_now);
    format_bytecount(dl_rate_buf, sizeof(dl_rate_buf), dl_rate);
    
    int printed = fprintf(stderr, "\33[2K\r    %s %s %s/s %02ld:%02ld", dl->filename, dl_total_buf, dl_rate_buf, t_since / 60, t_since % 60);
    if(printed < 0)
        geode_throw(dl->context, geode_io_ex(dl->context, errno, "fprintf(stderr) failed"));

    int space = dl->context->errstream.termsize.ws_col - printed - 8;
    if(space > PROGRESS_BAR_MIN_WIDTH) {
        space -= fprintf(stderr, "        [");

        int segments = dl_promille * space / 1000;        
        for(int i = 0; i < space; i++) {
            fputc(" #"[i < segments], stderr);
        }

        fputc(']', stderr);
    }

    fprintf(stderr, " % 3ld.%01ld%%", dl_promille / 10, dl_promille % 10);
    fflush(stderr);

    if(dl->t_last != t_now) {
        dl->t_last = t_now;
        dl->dl_last = dl_now;
    }
}

struct shard_value builtin_net_downloadFile(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct geode_context *context = e->ctx->userp;
    net_lazy_init(context);

    struct shard_value filename = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value url = shard_builtin_eval_arg(e, builtin, args, 1);
    
    FILE *fp = fopen(filename.path, "wb");
    if(!fp)
        geode_throw(context, geode_io_ex(context, errno, "Could not create file `%s'", filename.path));

    geode_infof(context, "Downloading from %s...", url.string);

    geode_update_termsize(context);

    curl_easy_setopt(curl, CURLOPT_URL, url.string);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if(context->outstream.isatty) {
        struct download_context dl = {
            .context = context,
            .filename = filename.string,
            .t_begin = time(NULL),
            .t_last = time(NULL),
            .dl_last = 0lu,
        };

        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &dl);
    }

    CURLcode err;
    if((err = curl_easy_perform(curl)) != CURLE_OK) {
        if(context->errstream.isatty)
            fprintf(stderr, "\n\n");
        fclose(fp);
        geode_throwf(context, GEODE_EX_NET, "Could not fetch `%s': %s", url.string, curl_easy_strerror(err));
    }

    if(context->errstream.isatty)
        fprintf(stderr, "\n\n");
    fclose(fp);

    return filename;
}

