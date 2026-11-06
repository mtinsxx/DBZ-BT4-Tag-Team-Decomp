#define _GNU_SOURCE
#include "bt4_guest_abi.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

void *Port_Window;
__attribute__((constructor(101))) static void configure(void) {
    Dl_info info;
    assert(dladdr(&Port_Window, &info) != 0);
    Port_Window = (void *)((uintptr_t)info.dli_fbase - 0x20000000u);
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
uint32_t D_002D3FE4, D_002D3FE8, D_002D97F4, D_002D97F8, D_002E6F5C, D_002E6F60;
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
int main(void) {
    uint32_t offset = 99;
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
