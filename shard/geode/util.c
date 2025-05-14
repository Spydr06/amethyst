#include "util.h"
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

