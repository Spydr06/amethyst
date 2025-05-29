#include "archives.h"

#include "exception.h"
#include "log.h"

#include <archive.h>
#include <archive_entry.h>

static int copy_data(struct archive *ar, struct archive *aw) {
    const void *buff;
    size_t size;
    la_int64_t offset;

    int err;
    while(true) {
        if((err = archive_read_data_block(ar, &buff, &size, &offset)) == ARCHIVE_EOF)
            return ARCHIVE_OK;
        else if(err < ARCHIVE_OK)
            return err; 

        if((err = archive_write_data_block(aw, buff, size, offset)) < ARCHIVE_OK)
            return err;
    }

    return 0;
}

struct shard_value builtin_archive_extractTar(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct geode_context *context = e->ctx->userp;

    struct shard_value src_path = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value dst_path = shard_builtin_eval_arg(e, builtin, args, 1);

    // FIXME: respect `dst_path`!
    geode_verbosef(context, "Extracting `%s' to `%s'", src_path.path, dst_path.path);

    struct archive *a = archive_read_new();
    struct archive *ext = archive_write_disk_new();

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
    archive_write_disk_set_standard_lookup(ext);

    int err;
    exception_t *ex = NULL;

    if((err = archive_read_open_filename(a, src_path.path, 10240))) {
        ex = geode_archive_ex(context, a, "Could not open archive `%s'", src_path.path);
        goto cleanup;
    }

    struct archive_entry *entry;
    while((err = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        if((err = archive_write_header(ext, entry)) < ARCHIVE_OK) {
            ex = geode_archive_ex(context, ext, "Could not extract `%s'", archive_entry_pathname(entry));
            goto cleanup;
        }
        else if(archive_entry_size(entry) > 0 && (err = copy_data(a, ext)) < ARCHIVE_OK) {
            ex = geode_archive_ex(context, ext, "Could not copy data of entry `%s'", archive_entry_pathname(entry));
        }

        if((err = archive_write_finish_entry(ext)) < ARCHIVE_OK) {
            ex = geode_archive_ex(context, ext, "Could not write `%s'", archive_entry_pathname(entry));
        }
    }

cleanup:
    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    if(ex)
        geode_throw(context, ex);

    return dst_path;
}

