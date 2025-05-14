#include "exception.h"
#include "package.h"
#include "log.h"
#include "util.h"
#include "defaults.h"

#include <errno.h>
#include <memory.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

struct directory {
    const char *path;
    DIR *dir;
    int dirfd;
};

static void open_directory(struct geode_context *context, struct directory *d, const char *path) {
    d->path = path;

    d->dirfd = open(path, O_RDONLY);
    if(d->dirfd < 0)
        geode_throw(context, geode_io_ex(context, errno, "Failed opening directory `%s'", path));

    d->dir = fdopendir(d->dirfd);
    if(!d->dir) {
        close(d->dirfd);
        d->dirfd = 0;
        geode_throw(context, geode_io_ex(context, errno, "Failed opening directory `%s'", path));
    }
}

static void close_directory(struct directory *d) {
    if(d->dir)
        closedir(d->dir);
    else if(d->dirfd >= 0)
        close(d->dirfd);

    d->dir = NULL;
    d->dirfd = -1;
}

static bool is_shard_file(struct dirent *entry) {
    return (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) && strendswith(entry->d_name, SHARD_FILE_EXT);
}

static void index_add_package(struct geode_context *context, struct geode_package_index *index, package_t *package) {

}

static void index_dir(struct geode_context *context, struct geode_package_index *index, struct directory *d) {
    struct dirent *entry;

    while(!(errno = 0) && (entry = readdir(d->dir))) {
        assert(entry->d_name);

        if(entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char *path = l_sprintf(&index->lifetime, "%s/%s", d->path, entry->d_name);
            struct directory dir;
            open_directory(context, &dir, path);
            index_dir(context, index, &dir);
            close_directory(&dir);
        }

        if(!is_shard_file(entry))
            continue;

        char *filepath = l_sprintf(&index->lifetime, "%s/%s", d->path, entry->d_name);

        package_t *package = geode_load_package_file(context, index, filepath);
        index_add_package(context, index, package);
    }

    if(errno)
        geode_throw(context, geode_io_ex(context, errno, "Failed reading package directory"));
}

void geode_mkindex(struct geode_package_index *index) {
    memset(index, 0, sizeof(struct geode_package_index));    
}

void geode_index_packages(struct geode_context *context, struct geode_package_index *index) {
    memset(index, 0, sizeof(struct geode_package_index));
    
    const char *pkgs_path = context->pkgs_path.string;
    geode_verbosef(context, "Indexing packages [%s]...", pkgs_path);

    struct directory dir = {0};

    volatile exception_t *e = geode_catch(context, GEODE_EX_IO | GEODE_EX_SHARD);
    if(e) {
        geode_pop_exception_handler(context);
        close_directory(&dir);
        geode_throw(context, e);
    }

    open_directory(context, &dir, pkgs_path);
    
    index_dir(context, index, &dir);

    geode_verbosef(context, "[%s] %zu available packages.", pkgs_path, index->index.size);

    close_directory(&dir);
    geode_pop_exception_handler(context);
}

