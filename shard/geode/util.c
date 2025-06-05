#include "util.h"
#include "include/defaults.h"
#include "lifetime.h"

#include <alloca.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool strendswith(const char *str, const char *suffix) {
    if(!str || !suffix)
        return false;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if(suffix_len > str_len)
        return false;
    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

char *strtrim(char *str) {
    while(isspace(*str))
        str++;

    size_t size = strlen(str);

    while(size > 0 && isspace(str[size - 1]))
        str[--size] = '\0';

    return str;
}

bool fexists(const char *filepath) {
    return access(filepath, F_OK) == 0; 
}

bool fwritable(const char *filepath) {
    return access(filepath, W_OK) == 0;
}

bool freadable(const char *filepath) {
    return access(filepath, R_OK) == 0;
}

bool fexecutable(const char *filepath) {
    return access(filepath, X_OK) == 0;
}

bool fisdir(const char *filepath) {
    struct stat _stat;
    if(stat(filepath, &_stat) < 0)
        return false;

    return (_stat.st_mode & S_IFMT) == S_IFDIR;
}

int copy_file(const char *src_path, const char *dst_path) {
    struct stat _stat;
    if(stat(src_path, &_stat) < 0)
        return errno;

    int fd_in = open(src_path, O_RDONLY);
    if(!fd_in)
        return errno;

    int fd_out = open(dst_path, O_WRONLY | O_CREAT, _stat.st_mode & 0777);
    if(!fd_out) {
        close(fd_in);
        return errno;
    }
    
    int err = copy_file_fd(fd_in, fd_out);

    close(fd_in);
    close(fd_out);

    return err;
}

int copy_file_fd(int fd_in, int fd_out) {
    char *block = alloca(BUFSIZ);
    ssize_t bytes_read = 0;
    while((bytes_read = read(fd_in, block, BUFSIZ)) > 0) {
        if(write(fd_out, block, bytes_read) < 0)
            return errno;
    }

    return bytes_read < 0 ? errno : 0;
}

static int _mkdir_recursive(const char *path, const char *seek, int mode) {
    while(seek[0] == '/')
        seek++;
    char *sep = strchrnul(seek, '/');
    
    char prev = sep[0];
    sep[0] = '\0';

    int err = 0;

    if(!fexists(path) && mkdir(path, mode) < 0)
        err = errno;

    sep[0] = prev;

    if(!err && prev)
        err = _mkdir_recursive(path, sep + 1, mode);

    return err;
}

int mkdir_recursive(const char *path, int mode) {
    return _mkdir_recursive(path, path, mode);
}

int copy_recursive(const char *src_path, const char *dst_path) {
    struct stat stat_in;
    if(stat(src_path, &stat_in) < 0)
        return errno;

    int fd_in = open(src_path, O_RDONLY);
    if(fd_in < 0)
        return errno;

    int err = mkdir_recursive(dst_path, stat_in.st_mode & 0777);
    if(err) {
        close(fd_in);
        return err;
    }

    int fd_out = open(dst_path, O_RDONLY);
    if(fd_out < 0) {
        close(fd_in);
        return errno;
    }

    int res = copy_recursive_fd(fd_in, fd_out);

    close(fd_in);
    close(fd_out);

    return res;
}

int copy_recursive_fd(int fd_in, int fd_out) {
    DIR *d_in = fdopendir(fd_in);
    if(!d_in)
        return errno;

    int err = 0;

    DIR *d_out = fdopendir(fd_out);
    if(!d_out) {
        err = errno;
        goto cleanup;
    }

    struct dirent *entry;
    while((entry = readdir(d_in))) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        struct stat ent_stat;
        if(fstatat(fd_in, entry->d_name, &ent_stat, 0) < 0) {
            err = errno;
            goto cleanup;
        }

        switch(ent_stat.st_mode & S_IFMT) {
        case S_IFREG:
            int ent_fd_in = openat(fd_in, entry->d_name, O_RDONLY);
            if(!ent_fd_in) {
                err = errno;
                goto cleanup;
            }

            int ent_fd_out = openat(fd_out, entry->d_name, O_WRONLY | O_CREAT, ent_stat.st_mode & 0777);
            if(!ent_fd_out) {
                err = errno;
                close(ent_fd_in);
                goto cleanup;
            }

            err = copy_file_fd(ent_fd_in, ent_fd_out);

            close(ent_fd_out);
            close(ent_fd_in);

            if(err)
                goto cleanup;
            break;
        case S_IFDIR: {
            int ent_fd_in = openat(fd_in, entry->d_name, O_RDONLY);
            if(!ent_fd_in) {
                err = errno;
                goto cleanup;
            }

            if(mkdirat(fd_out, entry->d_name, ent_stat.st_mode & 0777) < 0) {
                err = errno;
                goto cleanup;
            }

            int ent_fd_out = openat(fd_out, entry->d_name, O_RDONLY);
            if(!ent_fd_out) {
                err = errno;
                close(ent_fd_in);
             goto cleanup;
            }

            err = copy_recursive_fd(ent_fd_in, ent_fd_out);

            close(ent_fd_in);
            close(ent_fd_out);

            if(err)
                goto cleanup;
        } break;
        default:
            assert("unimplemented!\n");
            // TODO
            break;
        }
    }

cleanup:
    if(d_in)
        closedir(d_in);
    if(d_out)
        closedir(d_out);

    return err;
}

int rmdir_recursive(const char *path) {
    DIR *dir = opendir(path);
    if(!dir)
        return errno;

    int err = 0;
    char path_buf[PATH_MAX + 1];
    struct dirent *entry;
    while((entry = readdir(dir))) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        strncpy(path_buf, path, PATH_MAX);
        strncat(path_buf, "/", PATH_MAX);
        strncat(path_buf, entry->d_name, PATH_MAX);

        switch(entry->d_type) {
        case DT_DIR:
            err = rmdir_recursive(path_buf);
            if(err)
                goto cleanup;
            break;
        default:
            if(remove(path_buf) < 0) {
                err = errno;
                goto cleanup;
            }
        }
    }

cleanup:
    closedir(dir);

    if(err)
        return err;

    return remove(path) ? errno : 0;
}

ssize_t read_whole_file(lifetime_t *l, const char *path, char **bufptr) {
    struct stat s;
    if(stat(path, &s) < 0)
        return -errno;

    (*bufptr) = l_malloc(l, s.st_size + sizeof(char));
    if(!*bufptr)
        return -ENOMEM;

    FILE *f = fopen(path, "rb");
    if(!f)
        return -errno;

    size_t r = fread(*bufptr, 1, s.st_size, f);
    if(r != (size_t) s.st_size)
        return -errno;

    (*bufptr)[r] = '\0';
    return r;
}

char *geode_getcwd(lifetime_t *l) {
    long path_max = pathconf(".", _PC_PATH_MAX);
    if(path_max < 0)
        return 0;

    char *cwd = l_malloc(l, (path_max + 1) * sizeof(char)); 
    return getcwd(cwd, path_max + 1);
}

int geode_pushd(struct geode_context *context, const char *path) {
    int dfd = open(path, O_RDONLY);
    if(dfd < 0)
        return errno;

    if(context->dirstack.capacity < context->dirstack.count + 1) {
        size_t new_capacity = MAX(context->dirstack.capacity * 2, 32);
        int *new_dfds = realloc(context->dirstack.dfds, new_capacity * sizeof(int));
        if(!new_dfds) {
            close(dfd);
            return errno;
        }

        context->dirstack.dfds = new_dfds;
        context->dirstack.capacity = new_capacity;
    }

    if(fchdir(dfd) < 0) {
        close(dfd);
        return errno;
    }

    context->dirstack.dfds[context->dirstack.count++] = dfd;

    return 0;
}

int geode_popd(struct geode_context *context) {
    switch(context->dirstack.count) {
    case 0:
        return EOF;
    case 1:
        return close(context->dirstack.dfds[--context->dirstack.count]) < 0 ? errno : 0;
    default:
        if(fchdir(context->dirstack.dfds[context->dirstack.count - 2]) < 0)
            return errno;
        return close(context->dirstack.dfds[--context->dirstack.count]) < 0 ? errno : 0;
    }
}

char *geode_tmpfile(struct geode_context *context, const char *name) {
    if(context->tmpfiles.capacity < context->tmpfiles.count + 1) {
        size_t new_capacity = MAX(context->tmpfiles.capacity * 2, 32);
        char **new_tmpfiles = realloc(context->tmpfiles.items, new_capacity * sizeof(char*));
        if(!new_tmpfiles)
            geode_throw(context, geode_io_ex(context, errno, "failed allocating tmpfile buffer"));
        
        context->tmpfiles.items = new_tmpfiles;
        context->tmpfiles.capacity = new_capacity;
    }

    char *tmpname = l_sprintf(&context->l_global, DEFAULT_TMPFILE_FORMAT, getpid(), rand(), name);

    if(fexists(tmpname))
        geode_throw(context, geode_io_ex(context, EEXIST, "temporary file `%s'", tmpname));

    context->tmpfiles.items[context->tmpfiles.count++] = tmpname;
    return tmpname;
}

void geode_unlink_tmpfiles(struct geode_context *context) {
    for(size_t i = 0; i < context->tmpfiles.count; i++) {
        if(fexists(context->tmpfiles.items[i]))
            unlink(context->tmpfiles.items[i]);
    }

    free(context->tmpfiles.items);
    context->tmpfiles.items = NULL;
    context->tmpfiles.count = 0;
    context->tmpfiles.capacity = 0;
}

const char *inplace_basename(const char *path) {
    if(!path)
        return NULL;

    char *last_sep = strrchr(path, '/');
    return last_sep
        ? last_sep + 1
        : path;
}

