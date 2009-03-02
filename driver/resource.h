#ifndef __sys_resource_h

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/time.h>

#define __sys_resource_h 1

int getrusage(int, struct rusage*);
int getrlimit (int resource, struct rlimit *rlp);
#ifndef VMS
int setrlimit _G_ARGS((int resource, const struct rlimit *rlp));
#endif
long      ulimit(int, long);
int       getpriority(int, int);
int       setpriority(int, int, int);

#ifdef __cplusplus
}
#endif

#endif 
