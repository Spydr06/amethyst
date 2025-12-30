#ifndef _AMETHYST_CRED_H
#define _AMETHYST_CRED_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_AMETHYST_KERNEL_SRC) || defined(_AMETHYST_MODULE_SRC)
    #include <abi.h>
#else
    #include <bits/alltypes.h>
#endif

#define AMETHYST_ROOT_UID 0
#define AMETHYST_ROOT_GID 0

struct amethyst_cred {
    uid_t uid;
    gid_t gid;
};

#define AMETHYST_ROOT_CRED ((struct amethyst_cred){ \
        .uid = AMETHYST_ROOT_UID                    \
        .gid = AMETHYST_ROOT_GID                    \
    })

#ifdef __cplusplus
}
#endif

#endif /* _AMETHYST_CRED_H */
