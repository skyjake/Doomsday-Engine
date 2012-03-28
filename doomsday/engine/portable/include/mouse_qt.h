#ifndef MOUSE_QT_H
#define MOUSE_QT_H

#include "sys_input.h"

#ifdef __cplusplus
extern "C" {
#endif

extern mouseinterface_t qtMouse;

/**
 * Submits a new mouse event for preprocessing. The event has likely just been
 * received from the windowing system.
 *
 * @param button  Which button.
 * @param isDown  Is the button pressed or released.
 */
void Mouse_Qt_SubmitButton(int button, boolean isDown);

/**
 * Submits a new motion event for preprocessing.
 *
 * @param axis    Which axis.
 * @param deltaX  Horizontal delta.
 * @param deltaY  Vertical delta.
 */
void Mouse_Qt_SubmitMotion(int axis, int deltaX, int deltaY);

/**
 * Submits an absolute mouse position for the UI mouse mode.
 *
 * @param x  X coordinate. 0 is at the left edge of the window.
 * @param y  Y coordinate. 0 is at the top edge of the window.
 */
void Mouse_Qt_SubmitWindowPosition(int x, int y);

#ifdef __cplusplus
}
#endif

#endif // MOUSE_QT_H
