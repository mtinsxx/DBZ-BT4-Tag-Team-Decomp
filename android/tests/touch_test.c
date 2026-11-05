#include "android_touch.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

/* Tests the input bridge only, never claims gameplay has been run. */
static void test_neutral_and_invalid(void) {
    Bt4TouchPad out;
    Bt4Touch_ClearAll();
    assert(!Bt4Touch_Read(0, &out));
    assert(out.buttons == 0);
    assert(out.lx == 128 && out.ly == 128);
    assert(out.rx == 128 && out.ry == 128);
    assert(out.active_axes == 0);
    assert(!Bt4Touch_Read(-1, &out));
    assert(!Bt4Touch_Read(2, &out));
    assert(!Bt4Touch_Read(0, NULL));
    Bt4Touch_Publish(-1, &out);
    Bt4Touch_Clear(3);
}

static void test_publish_buttons_axes_clear(void) {
    Bt4TouchPad out;
    const Bt4TouchPad pressed = { 0xC000, 70, 90, 20, 230, 0xFF };
    Bt4Touch_Publish(0, &pressed);
    assert(Bt4Touch_Read(0, &out));
    assert(out.buttons == 0xC000);
    assert(out.rx == 70 && out.ry == 90);
    assert(out.lx == 20 && out.ly == 230);
    assert(out.active_axes == (BT4_TOUCH_LEFT_AXIS | BT4_TOUCH_RIGHT_AXIS));
    assert(!Bt4Touch_Read(1, &out));
    Bt4Touch_Clear(0);
    assert(!Bt4Touch_Read(0, &out));
    assert(out.buttons == 0 && out.lx == 128);
}

static void *writer(void *unused) {
    int i;
    const Bt4TouchPad a = { 0x4000, 128, 128, 10, 20, BT4_TOUCH_LEFT_AXIS };
    const Bt4TouchPad b = { 0x8000, 128, 128, 30, 40, BT4_TOUCH_LEFT_AXIS };
    (void)unused;
    for (i = 0; i < 20000; ++i) Bt4Touch_Publish(0, i & 1 ? &a : &b);
    return NULL;
}

static void test_atomic_snapshot(void) {
    pthread_t thread;
    Bt4TouchPad out;
    int i;
    Bt4Touch_ClearAll();
    assert(pthread_create(&thread, NULL, writer, NULL) == 0);
    for (i = 0; i < 20000; ++i) {
        if (Bt4Touch_Read(0, &out)) {
            if (out.buttons == 0x4000) {
                assert(out.lx == 10 && out.ly == 20);
            } else {
                assert(out.buttons == 0x8000);
                assert(out.lx == 30 && out.ly == 40);
            }
        }
    }
    assert(pthread_join(thread, NULL) == 0);
    Bt4Touch_ClearAll();
}

int main(void) {
    test_neutral_and_invalid();
    test_publish_buttons_axes_clear();
    test_atomic_snapshot();
    puts("Touch bridge: all unit tests passed (not a game test).");
    return 0;
}
