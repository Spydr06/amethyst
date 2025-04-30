#include <errno.h>
#include <stdio.h>
#include <sys/syscall.h>

#include <internal/file.h>
#include <internal/syscall.h>

off_t __file_seek(FILE *f, off_t off, int whence) {
    return syscall(SYS_lseek, f->fd, off, whence);
}

int fseek(FILE *stream, long off, int whence) {
    // TODO: lock & unlock stream
    
    if(whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        errno = EINVAL;
        return -1;
    }

	// adjust relative offset for unread data in buffer
	if (whence == SEEK_CUR && stream->rend)
        off -= stream->rend - stream->rpos;

	// flush write buffer
	if (stream->wpos != stream->wbase) {
		stream->write(stream, 0, 0);
		if (!stream->wpos) return -1;
	}

	stream->wpos = stream->wbase = stream->wend = 0;

	// Perform the underlying seek.
	if (stream->seek(stream, off, whence) < 0) return -1;

	// if seek succeeded, file is seekable => discard read buffer
	stream->rpos = stream->rend = 0;
	stream->flags &= ~F_EOF;
	
	return 0;
}

