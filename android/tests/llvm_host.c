#define _GNU_SOURCE
#include "bt4_guest_abi.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

void *Port_Window;
__attribute__((constructor(101))) static void configure(void) {
    Dl_info info;
    assert(dladdr(&Port_Window, &info) != 0);
    const char *offset=getenv("BT4_TEST_WINDOW_OFFSET");
    uintptr_t distance=offset?(uintptr_t)strtoull(offset,NULL,0):0x20000000u;
    Port_Window = (void *)((uintptr_t)info.dli_fbase - distance);
}
extern int *guest_value(void);
extern uint32_t guest_address(void);
extern int guest_read(void), guest_null(void), guest_indirect(void);
extern int guest_direct(void), guest_foreign(int (*)(int *), int *);
extern float guest_fp_native(float);
extern float guest_fp_indirect(float), native_scale(float);
extern int guest_fp_variadic(void);
extern int guest_fp_format(void), guest_rejected_varcallback(void);
extern int guest_sdk_contracts(void);
extern int guest_sdk_ipu_callbacks(void);
uint32_t D_002D3FE4, D_002D3FE8, D_002D97F4, D_002D97F8, D_002E6F5C, D_002E6F60;
/* Synthetic imported resources and a guest-addressable heap fixture for the
   actual IPU diagnostic adapter. The native host stack may be out of range. */
const char D_002FA6D0[] = "test: %s\n";
const char D_002FA950[] = "A", D_002FA980[] = "B", D_002FA998[] = "C", D_002FA9D0[] = "D";
static uint32_t sdk_event[2];
static int sdk_event_used;
uint32_t phi_value;
extern uint32_t guest_phi_edge(int), guest_phi_switch(int), guest_phi_checked_edge(int);
void *Port_LowAlloc(size_t size) {
    assert(size == sizeof(sdk_event) && !sdk_event_used);
    sdk_event_used = 1;
    return sdk_event;
}
void Port_LowFree(void *pointer) {
    assert(pointer == sdk_event && sdk_event_used);
    sdk_event_used = 0;
}
static int foreign_callback(int *p) { return *p + 1; }
extern void guest_write(int), guest_copy(void);
typedef struct { uint32_t prev, next; } GameNode;
typedef struct { uint32_t head, tail; int32_t count; } GameList;
static GameList list;
static GameNode first, second;
extern void List_Init(GameList *), List_PushFront(GameList *, GameNode *), List_PushBack(GameList *, GameNode *);
extern GameNode *List_GetHead(GameList *), *List_GetTail(GameList *), *List_PopFront(GameList *), *List_PopBack(GameList *);
extern int List_GetCount(GameList *);
extern uint32_t bt4_test_data_table[];
extern uint32_t guest_absolute_address(void);
extern void *guest_absolute_host(void);
int main(void) {
    uint32_t offset = 99;
    assert(guest_absolute_address()==0x3be71c);
    assert(guest_absolute_host()==(char *)Port_Window+0x3be71c);
    int outside_window = 11;
    assert(Bt4Android_TryEncodePointer(NULL, &offset) == 0 && offset == 0);
    assert(Bt4Android_DecodePointer(0) == NULL);
    assert(Bt4Android_DecodePointer(0x20000000u) == (char *)Port_Window + 0x20000000u);
    assert(Bt4Android_TryEncodePointer(Port_Window, &offset) == -1);
    assert(Bt4Android_TryEncodePointer((char *)Port_Window - 1, &offset) == -1);
    assert(Bt4Android_TryEncodePointer((char *)Port_Window + UINT64_C(0x100000000), &offset) == -1);
    assert(Bt4Android_TryEncodePointer((void *)((uintptr_t)Port_Window + UINT32_MAX), &offset) == 0 && offset == UINT32_MAX);
    assert(guest_address() == (uintptr_t)guest_value() - (uintptr_t)Port_Window);
    assert(guest_read() == 82);
    guest_write(17);
    assert(guest_read() == 34);
    guest_copy();
    assert(guest_read() == 36);
    assert(guest_null());
    assert(guest_indirect());
    assert(guest_direct());
    assert(guest_sdk_contracts());
    assert(guest_sdk_ipu_callbacks() && !sdk_event_used);
    assert(guest_phi_edge(0) == 0 && guest_phi_edge(1) == Bt4Android_EncodePointer(&phi_value));
    assert(guest_phi_switch(0) == 0 && guest_phi_switch(3) == 0);
    assert(guest_phi_switch(1) == Bt4Android_EncodePointer(&phi_value));
    assert(guest_phi_switch(2) == Bt4Android_EncodePointer(&phi_value));
    assert(guest_phi_checked_edge(0) == 0);
    assert(guest_foreign(foreign_callback, &outside_window) == 12);
    assert(guest_fp_native(3.5f) == 7.0f && guest_fp_variadic() == 4);
    assert(guest_fp_format() == 4);
    assert(guest_fp_indirect(3.5f) == 3.5f);
    {
        pid_t child = fork();
        int status;
        assert(child >= 0);
        if (child == 0) {
            const struct rlimit limit = {0, 0};
            setrlimit(RLIMIT_CORE, &limit);
            guest_rejected_varcallback();
            _exit(99);
        }
        assert(waitpid(child, &status, 0) == child);
        assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    }
    {
        pid_t child = fork();
        int status;
        assert(child >= 0);
        if (child == 0) {
            const struct rlimit limit = {0, 0};
            setrlimit(RLIMIT_CORE, &limit);
            guest_phi_checked_edge(1);
            _exit(99);
        }
        assert(waitpid(child, &status, 0) == child);
        assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    }
    assert(bt4_test_data_table[0] == Bt4Android_EncodePointer((const void *)guest_value));
    assert(bt4_test_data_table[1] == Bt4Android_EncodePointer(&bt4_test_data_table[1]));
    assert(bt4_test_data_table[2] == 0x12345678);
    List_Init(&list);
    List_PushBack(&list, &second);
    List_PushFront(&list, &first);
    assert(List_GetCount(&list) == 2);
    assert(List_GetHead(&list) == &first && List_GetTail(&list) == &second);
    assert(first.next == Bt4Android_EncodePointer(&second));
    assert(second.prev == Bt4Android_EncodePointer(&first));
    assert(List_PopFront(&list) == &first && List_PopBack(&list) == &second);
    assert(List_GetCount(&list) == 0 && List_GetHead(&list) == NULL);
    printf("ELF guest ABI: layouts, initializers, memory, callbacks, NULL, range checks passed (W=%p)\n", Port_Window);
    return 0;
}
