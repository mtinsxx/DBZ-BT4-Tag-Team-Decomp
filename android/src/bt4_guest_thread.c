#include "bt4_guest_thread.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef struct ThreadCall { int (*entry)(void *);void *context;int result; } ThreadCall;
static void *run(void *context) {
    ThreadCall *call=context;
    call->result=call->entry(call->context);
    return NULL;
}
static int failed(Bt4ThreadTrace *trace,const char *operation,int error) {
    if(trace) { trace->operation=operation;trace->error=error; }
    errno=error;return -1;
}
int Bt4Thread_StackValid(void *provided,size_t capacity,void *registered,
                         size_t usable,void *frame) {
    uintptr_t base=(uintptr_t)provided,value=(uintptr_t)frame;
    return provided && registered==provided && usable && usable<=capacity &&
        capacity<=UINTPTR_MAX-base && value>=base && value-base<usable && !(value&15);
}
int Bt4Thread_Run(void *stack,size_t bytes,int (*entry)(void *),void *context,
                  int *result,Bt4ThreadTrace *trace) {
    pthread_attr_t attributes;pthread_t thread;int error;
    ThreadCall call={entry,context,0};
    if(trace) { trace->operation="validate";trace->error=0; }
    if(!stack || !bytes || !entry || !result)return failed(trace,"validate",EINVAL);
    error=pthread_attr_init(&attributes);
    if(error)return failed(trace,"pthread_attr_init",error);
    error=pthread_attr_setguardsize(&attributes,0); /* The reservation is caller-owned. */
    if(error) { pthread_attr_destroy(&attributes);return failed(trace,"pthread_attr_setguardsize",error); }
    error=pthread_attr_setstack(&attributes,stack,bytes);
    if(error) { pthread_attr_destroy(&attributes);return failed(trace,"pthread_attr_setstack",error); }
    error=pthread_create(&thread,&attributes,run,&call);
    pthread_attr_destroy(&attributes);
    if(error)return failed(trace,"pthread_create",error);
    error=pthread_join(thread,NULL);
    if(error) {
        fprintf(stderr,"guest thread fatal: pthread_join errno=%d; stack/context still in use\n",error);
        abort();
    }
    *result=call.result;
    if(trace)trace->operation="joined";
    return 0;
}
