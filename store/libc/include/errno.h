#ifndef _ERRNO_H
#define _ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

#define errno (*__errno_location())
int* __errno_location(void);

#ifdef __cplusplus
}
#endif

#endif /* _ERRNO_H */

