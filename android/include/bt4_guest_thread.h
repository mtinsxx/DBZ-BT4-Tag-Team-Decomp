#ifndef BT4_GUEST_THREAD_H
#define BT4_GUEST_THREAD_H
#include <stddef.h>
typedef struct Bt4ThreadTrace {
    const char *operation;
    int error;
} Bt4ThreadTrace;
/* Runs entry on an OS-registered caller-owned stack. The stack and context
   remain alive until the thread is joined. Returns 0 or -1 with pthread's
   actual error code; the entry's own result is separate. A join invariant
   failure terminates rather than returning with a live stack user. */
/* Bionic may carve pthread metadata from the supplied stack's high end.
   Validate the effective range and actual frame, not equality of sizes. */
int Bt4Thread_StackValid(void *provided,size_t capacity,void *registered,
                         size_t usable,void *frame);
int Bt4Thread_Run(void *stack,size_t bytes,int (*entry)(void *),void *context,
                  int *result,Bt4ThreadTrace *trace);
#endif
