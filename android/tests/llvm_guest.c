/* Compiled through the production LLVM pass, not the host test compiler. */
typedef unsigned int u32;
typedef int * __ptr32 __uptr GuestPtr;
typedef struct Node { GuestPtr value; struct Node * __ptr32 __uptr next; } Node;
_Static_assert(sizeof(GuestPtr) == 4, "guest pointer width");
_Static_assert(sizeof(Node) == 8, "guest structure layout");
static int value = 41;
static Node node = { &value, &node };
static GuestPtr pointer = &value;
static int (* __ptr32 __uptr callback)(int *) = 0;

int *guest_value(void) { return &value; }
u32 guest_address(void) { return (u32)&value; }
int guest_read(void) { return *node.next->value + *pointer; }
void guest_write(int n) { *node.next->value = n; }
int guest_null(void) { GuestPtr p = 0; return (void *)p == 0; }
void guest_copy(void) { Node copy; __builtin_memcpy(&copy, &node, sizeof(Node)); *copy.value += 1; }

static int accept_pointer(int *p) { return p == &value ? *p : -1; }
static int accept_integer(u32 p) { return p == (u32)&value ? 73 : -1; }
static u32 return_integer(void) { return (u32)&value; }
static int *return_pointer(void) { return &value; }
extern int *cross_address(void);
extern int cross_accept(u32);
extern int cross_fixed(int, ...);
int guest_direct(void) {
    return *cross_address() == value && cross_accept((u32)&value) == value &&
           cross_fixed(7, (u32)&value) == value + 7;
}
int guest_foreign(int (*fn)(int *), int *p) { return fn(p); }
extern float native_scale(float);
extern double native_variadic(int, ...);
float guest_fp_native(float x) { return native_scale(x); }
int guest_fp_variadic(void) { return (int)native_variadic(0, 1.5, 2.5); }
static float soft_identity(float x) { return x; }
float guest_fp_indirect(float x) {
    float (* __ptr32 __uptr fn)(float) = soft_identity;
    return ((__typeof__(*fn) *)(fn))(x);
}

/* Deliberately reproduce the engine's cross-TU/callback i32/pointer mismatch.
 * An arbitrary ASLR base makes simply taking the low 32 bits incorrect. */
int guest_indirect(void) {
    int a, b, c, d;
    int (* __ptr32 __uptr as_integer)(u32) = (int (*)(u32))accept_pointer;
    int *(* __ptr32 __uptr result_pointer)(void) = (int *(*)(void))return_integer;
    u32 (* __ptr32 __uptr result_integer)(void) = (u32 (*)(void))return_pointer;
    callback = (int (*)(int *))accept_integer;
    a = ((__typeof__(*callback) *)(callback))(&value);
    b = ((__typeof__(*as_integer) *)(as_integer))((u32)&value);
    c = *((__typeof__(*result_pointer) *)(result_pointer))();
    d = ((__typeof__(*result_integer) *)(result_integer))() == (u32)&value;
    return a == 73 && b == value && c == value && d;
}
