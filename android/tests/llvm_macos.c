/* Compile-only regression for the existing Mach-O mode, without a macOS SDK. */
struct Pair { int * __ptr32 __uptr pointer; int value; };
_Static_assert(sizeof(struct Pair) == 8, "desktop guest layout");
extern struct Pair pair;
int * __ptr32 __uptr desktop_pointer = &pair.value;
int desktop_read(void) { return *pair.pointer; }
