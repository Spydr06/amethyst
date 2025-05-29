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

void net_lazy_init(struct geode_context *) {
    if(!curl)
        curl = curl_easy_init();
    if(!curl)
        assert(!"curl init failed");
}

static size_t write_to_fp(void *ptr, size_t size, size_t nmemb, void *stream) {
    FILE *fp = (FILE*) stream;
    return fwrite(ptr, size, nmemb, fp);
}

struct shard_value builtin_net_downloadTmp(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct geode_context *context = e->ctx->userp;
    net_lazy_init(context);

    struct shard_value url = shard_builtin_eval_arg(e, builtin, args, 0);
    
    char *tmpfile = geode_tmpfile(context, inplace_basename(url.string));
    FILE *fp = fopen(tmpfile, "wb");
    if(!fp)
        geode_throw(context, geode_io_ex(context, errno, "Could not open temporary file `%s'", tmpfile));

    geode_verbosef(context, "Downloading `%s' to `%s'", url.string, tmpfile);

    curl_easy_setopt(curl, CURLOPT_URL, url.string);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_fp);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode err;
    if((err = curl_easy_perform(curl)) != CURLE_OK) {
        fclose(fp);
        geode_throwf(context, GEODE_EX_NET, "Could not fetch `%s': %s", url.string, curl_easy_strerror(err));
    }

    fclose(fp);

    return (struct shard_value){.type=SHARD_VAL_PATH, .path=tmpfile, .pathlen=strlen(tmpfile)};
}

