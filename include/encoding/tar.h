#ifndef _AMETHYST_FILEFORMAT_TAR_H
#define _AMETHYST_FILEFORMAT_TAR_H

#include <abi.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#define TAR_BLOCKSIZE 512

struct tar_header {
	uint8_t name[100];
	uint8_t mode[8];
	uint8_t uid[8];
	uint8_t gid[8];
	uint8_t size[12];
	uint8_t modtime[12];
	uint8_t checksum[8];
	uint8_t type[1];
	uint8_t link[100];
	uint8_t indicator[6];
	uint8_t version[2];
	uint8_t uname[32];
	uint8_t gname[32];
	uint8_t devmajor[8];
	uint8_t devminor[8];
	uint8_t prefix[155];
};

enum tar_entry_type : uint16_t {
    TAR_FILE,
    TAR_HARDLINK,
    TAR_SYMLINK,
    TAR_CHARDEV,
    TAR_BLOCK,
    TAR_DIR,
    TAR_FIFO
};

struct tar_entry {
	uint8_t name[257];
	mode_t mode;
	gid_t gid;
	uid_t uid;
	size_t size;
	struct timespec modtime;
	uint16_t checksum;
	enum tar_entry_type type;
	uint8_t link[101];
	uint8_t indicator[6];
	uint8_t version[2];
	uint16_t devminor;
	uint16_t devmajor;
};

const char* tar_entry_type_str(enum tar_entry_type type);

#endif /* _AMETHYST_FILEFORMAT_TAR_H */

