#ifndef _AMETHYST_SYS_TERMIOS_H
#define _AMETHYST_SYS_TERMIOS_H

#define VINTR 0
#define ISIG 0000001
#define NCCS 32
#define ECHO 0000010
#define ICANON 0000002
#define VTIME 5
#define VMIN 6
#define INLCR 0000100
#define IGNCR 0000200
#define ICRNL 0000400
#define OCRNL 0000010
#define ECHOCTL 0001000
#define ONLCR 0000004

#define TCGETS 0x5401
#define TCSETS 0x5402
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#endif /* _AMETHYST_SYS_TERMIOS_H */

