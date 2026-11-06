#define _GNU_SOURCE
#include "bt4_guest_thread.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
static _Thread_local int local=17;
typedef struct Context { void *stack;size_t bytes;pthread_t parent;int called; } Context;
static int entry(void *argument) {
    Context *context=argument;pthread_attr_t attributes;void *stack;size_t bytes;
    uintptr_t frame=(uintptr_t)__builtin_frame_address(0);
    assert(!pthread_equal(pthread_self(),context->parent));
    assert(!pthread_getattr_np(pthread_self(),&attributes));
    assert(!pthread_attr_getstack(&attributes,&stack,&bytes));pthread_attr_destroy(&attributes);
    assert(stack==context->stack && bytes==context->bytes);
    assert(frame>=(uintptr_t)stack && frame<(uintptr_t)stack+bytes && !(frame&15));
    assert(local==17);local=29;context->called=1;
    return 73;
}
int main(void) {
    Context context={.bytes=16*1024*1024,.parent=pthread_self()};
    context.stack=mmap(NULL,context.bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(context.stack!=MAP_FAILED);
    /* Bionic excludes pthread_internal_t from usable size (glibc need not).
       Accept that contained range, reject a frame in metadata/outside memory. */
    unsigned char *base=context.stack;
    assert(Bt4Thread_StackValid(base,context.bytes,base,context.bytes-4096,base+1024));
    assert(!Bt4Thread_StackValid(base,context.bytes,base,context.bytes-4096,base+context.bytes-16));
    assert(!Bt4Thread_StackValid(base,context.bytes,base,context.bytes+1,base+1024));
    assert(!Bt4Thread_StackValid(base,context.bytes,base+4096,context.bytes-4096,base+8192));
    assert(!Bt4Thread_StackValid(base,context.bytes,base,context.bytes,base+1025));
    local=61;int result=-99;Bt4ThreadTrace trace;
    assert(!Bt4Thread_Run(context.stack,context.bytes,entry,&context,&result,&trace));
    assert(context.called && result==73 && local==61 && !trace.error);
    context.called=0;
    assert(Bt4Thread_Run(context.stack,1,entry,&context,&result,&trace)==-1 && errno==EINVAL && !context.called);
    assert(Bt4Thread_Run(NULL,context.bytes,entry,&context,&result,&trace)==-1 && errno==EINVAL);
    assert(!munmap(context.stack,context.bytes));
    puts("Registered native thread: actual stack bounds, alignment, TLS, return, join and invalid size passed");
    return 0;
}
