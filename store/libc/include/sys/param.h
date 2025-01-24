#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

#define ABS(a) ((a) < 0 ? -(a) : (a))

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PARAM_H */

