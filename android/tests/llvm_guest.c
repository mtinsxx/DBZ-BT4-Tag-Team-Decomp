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
extern int native_format(int, ...);
float guest_fp_native(float x) { return native_scale(x); }
int guest_fp_variadic(void) { return (int)native_variadic(0, 1.5, 2.5); }
int guest_fp_format(void) { return native_format(0, 1.5, 2.5); }
int guest_rejected_varcallback(void) {
    int (* __ptr32 __uptr fn)(int, ...) = native_format;
    return ((__typeof__(*fn) *)(fn))(0, 1.5, 2.5);
}
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

/* Exercise the actual reconstructed SDK sources across the guest/native ABI,
   including native integer words that game declarations treat as pointers. */
static u32 sdk_source[4] = {0x80000000u, 0x12345678u, 0xFFFFFFFFu, 17};
static u32 sdk_destination[4], sdk_record[16];
extern u32 D_002D3FE4, D_002D3FE8;
extern int func_002BA2D0(void *, const void *, u32);
extern int func_002BA688(void *, const void *, u32);
extern void *func_002BA308(const void *, const void *, u32);
extern void func_0027BD48(void *, void *);
extern void func_00283338(void *, void *, void *);
extern void *func_00283330(const void *);
extern int func_0028C158(void);
int guest_sdk_contracts(void) {
    int (* __ptr32 __uptr size_query)(void *) = (int (*)(void *))func_0028C158;
    void *(* __ptr32 __uptr uuid_query)(const void *) = func_00283330;
    if (func_002BA2D0(sdk_destination, sdk_source, 15) != 0 ||
        sdk_destination[0] != sdk_source[0] || sdk_destination[2] != sdk_source[2] || sdk_destination[3] != 0) return 0;
    if (func_002BA688(sdk_destination, sdk_source, 16) != 0 || sdk_destination[3] != 17) return 0;
    if (func_002BA308(sdk_source, sdk_source + 3, 0xFFFFFFFFu) != sdk_source + 2) return 0;
    if (func_002BA308(sdk_source, sdk_source + 3, 17) != 0) return 0;
    func_00283338(sdk_record, (void *)accept_pointer, &value);
    if (sdk_record[14] != (u32)accept_pointer || sdk_record[15] != (u32)&value) return 0;
    sdk_record[2] = (u32)&value;
    if (func_00283330(sdk_record) != &value ||
        ((__typeof__(*uuid_query) *)(uuid_query))(sdk_record) != &value) return 0;
    func_0027BD48((void *)accept_pointer, &value);
    return D_002D3FE4 == (u32)accept_pointer && D_002D3FE8 == (u32)&value &&
        ((__typeof__(*size_query) *)(size_query))(sdk_record) == 2048;
}

/* Native SDK -> transformed game callbacks must preserve compact descriptors
   as well as each callback's actual pointer/integer declaration. */
static u32 sdk_decoder[0x870/4], sdk_outer[0x44/4], sdk_error_table[0x18/4];
static u32 sdk_expected_text;
static int sdk_callback_count;
extern const char D_002FA950[], D_002FA980[], D_002FA998[], D_002FA9D0[];
extern int func_002A0C00(const void *), func_002A0C10(const void *);
extern int func_002A0E08(const void *), func_002A0E18(const void *);
static int sdk_pointer_callback(void *outer, const u32 *event, u32 context) {
    if (outer != sdk_outer || event[0] != 0 || event[1] != sdk_expected_text || context != 0xF1234567u) return -1;
    ++sdk_callback_count;
    return (int)0x8ABCDEF0u;
}
static int sdk_integer_callback(u32 outer, u32 event_word, int *context) {
    const u32 *event = (const u32 *)event_word;
    if (outer != (u32)sdk_outer || event[0] != 0 || event[1] != sdk_expected_text || context != &value) return -1;
    ++sdk_callback_count;
    return (int)0x8ABCDEF0u;
}
int guest_sdk_ipu_callbacks(void) {
    int i,j;
    int (* __ptr32 __uptr methods[4])(const void *) = {func_002A0C00,func_002A0C10,func_002A0E08,func_002A0E18};
    const char * __ptr32 __uptr messages[4] = {D_002FA950,D_002FA980,D_002FA998,D_002FA9D0};
    sdk_decoder[0x868/4] = (u32)sdk_outer;
    sdk_decoder[0x0C/4] = 1;
    sdk_outer[0x40/4] = (u32)sdk_error_table;
    for (i=0;i<2;++i) {
        sdk_error_table[0x0C/4] = i ? (u32)sdk_integer_callback : (u32)sdk_pointer_callback;
        sdk_error_table[0x10/4] = i ? (u32)&value : 0xF1234567u;
        for (j=0;j<4;++j) {
            sdk_expected_text = (u32)messages[j];
            if (((__typeof__(*methods[j]) *)(methods[j]))(sdk_decoder) != (int)0x8ABCDEF0u) return 0;
        }
    }
    return sdk_callback_count == 8;
}
