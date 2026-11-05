/* Deliberate declaration differences are in llvm_guest.c, another TU. */
typedef unsigned int u32;
extern u32 guest_address(void);
u32 cross_address(void) { return guest_address(); }
int cross_accept(int *p) { return *p; }
int cross_fixed(int a, int *p) { return a + *p; }
