/* Thread-safe bridge for Android touchscreen input.
 * No SDL or JNI dependency: the platform UI and the game can run on
 * different threads without racing over button or stick values.
 */
#include "android_touch.h"
#include <pthread.h>
#include <string.h>

#define BT4_TOUCH_PLAYERS 2

static pthread_mutex_t sTouchLock = PTHREAD_MUTEX_INITIALIZER;
static Bt4TouchPad sTouch[BT4_TOUCH_PLAYERS];
static unsigned char sPresent[BT4_TOUCH_PLAYERS];

static int valid_player(int player) {
    return player >= 0 && player < BT4_TOUCH_PLAYERS;
}

static Bt4TouchPad neutral_pad(void) {
    Bt4TouchPad pad;
    memset(&pad, 0, sizeof(pad));
    pad.rx = pad.ry = pad.lx = pad.ly = 0x80;
    return pad;
}

void Bt4Touch_Publish(int player, const Bt4TouchPad *pad) {
    Bt4TouchPad clean;
    if (!valid_player(player) || pad == NULL) return;
    clean = *pad;
    clean.active_axes &= BT4_TOUCH_LEFT_AXIS | BT4_TOUCH_RIGHT_AXIS;
    pthread_mutex_lock(&sTouchLock);
    sTouch[player] = clean;
    sPresent[player] = 1;
    pthread_mutex_unlock(&sTouchLock);
}

void Bt4Touch_Clear(int player) {
    if (!valid_player(player)) return;
    pthread_mutex_lock(&sTouchLock);
    sTouch[player] = neutral_pad();
    sPresent[player] = 0;
    pthread_mutex_unlock(&sTouchLock);
}

void Bt4Touch_ClearAll(void) {
    int player;
    pthread_mutex_lock(&sTouchLock);
    for (player = 0; player < BT4_TOUCH_PLAYERS; player++) {
        sTouch[player] = neutral_pad();
        sPresent[player] = 0;
    }
    pthread_mutex_unlock(&sTouchLock);
}

int Bt4Touch_Read(int player, Bt4TouchPad *out) {
    int present;
    if (out == NULL) return 0;
    *out = neutral_pad();
    if (!valid_player(player)) return 0;
    pthread_mutex_lock(&sTouchLock);
    present = sPresent[player] != 0;
    if (present) *out = sTouch[player];
    pthread_mutex_unlock(&sTouchLock);
    return present;
}
