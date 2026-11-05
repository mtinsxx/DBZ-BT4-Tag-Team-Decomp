/* BT4 Android touchscreen -> original libpad2 input bridge.
 *
 * This is not an on-screen PS2 controller. Android UI code will translate
 * semantic game actions into a complete snapshot of the original pad state.
 * The existing game remains authoritative for combos and gameplay rules.
 */
#ifndef BT4_ANDROID_TOUCH_H
#define BT4_ANDROID_TOUCH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum {
    BT4_TOUCH_LEFT_AXIS  = 1u,
    BT4_TOUCH_RIGHT_AXIS = 2u
};

/* Buttons use the original libpad2 16-bit active-HIGH bit layout.
 * Port_PadRead converts them to the game's active-LOW report.
 * Axis values are 0..255, with 128 as the center.
 */
typedef struct Bt4TouchPad {
    uint16_t buttons;
    uint8_t rx, ry, lx, ly;
    uint8_t active_axes; /* which axes override a hardware controller */
} Bt4TouchPad;

/* Android UI thread: publish a complete state for player 0 or 1.
 * Use Clear on pointer cancellation, pause, focus loss and destruction.
 * Do not publish separate buttons/axes: one snapshot avoids torn combos.
 */
void Bt4Touch_Publish(int player, const Bt4TouchPad *pad);
void Bt4Touch_Clear(int player);
void Bt4Touch_ClearAll(void);

/* Game thread: returns 1 if a touchscreen state is available, 0 otherwise.
 * Never leaves an uninitialized pad in *out, even for invalid inputs.
 */
int Bt4Touch_Read(int player, Bt4TouchPad *out);

#ifdef __cplusplus
}
#endif
#endif
