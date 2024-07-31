#include <ff/tar.h>

const char* tar_entry_type_str(enum tar_entry_type type) {
    switch(type) {
        case TAR_FILE:
            return "file";
        case TAR_HARDLINK:
            return "hard link";
        case TAR_SYMLINK:
            return "symbolic link";
        case TAR_CHARDEV:
            return "char device";
        case TAR_BLOCK:
            return "block device";
        case TAR_DIR:
            return "directory";
        case TAR_FIFO:
            return "fifo";
        default:
            return "<unknown>";
    }
}
