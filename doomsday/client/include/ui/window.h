/** @file window.h
 * Window management. @ingroup base
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_SYS_WINDOW_H
#define LIBDENG_SYS_WINDOW_H

#ifndef __CLIENT__
#  error "window.h requires __CLIENT__"
#endif

#include "dd_types.h"
#include "resource/image.h"
#include "canvaswindow.h"

#define WINDOW_MIN_WIDTH        320
#define WINDOW_MIN_HEIGHT       240

/**
 * @defgroup consoleCommandlineFlags Console Command Line Flags
 * @ingroup flags
 * @{
 */
#define CLF_CURSOR_LARGE        0x1 // Use the large command line cursor.
/**@}*/

struct ddwindow_s; // opaque
typedef struct ddwindow_s Window;

// Doomsday window flags.
#define DDWF_VISIBLE            0x01
#define DDWF_CENTER             0x02
#define DDWF_MAXIMIZE           0x04
#define DDWF_FULLSCREEN         0x08

/**
 * Attributes for a window.
 */
enum windowattribute_e {
    DDWA_END = 0,           ///< Marks the end of an attribute list (not a valid attribute by itself)

    DDWA_X = 1,
    DDWA_Y,
    DDWA_WIDTH,
    DDWA_HEIGHT,
    DDWA_CENTER,
    DDWA_MAXIMIZE,
    DDWA_FULLSCREEN,
    DDWA_VISIBLE,
    DDWA_COLOR_DEPTH_BITS
};

/// Determines whether @a x is a valid window attribute id.
#define VALID_WINDOW_ATTRIBUTE(x)   ((x) >= DDWA_X && (x) <= DDWA_VISIBLE)

#if 0
// Flags for Sys_SetWindow()
#define DDSW_NOSIZE             0x01
#define DDSW_NOMOVE             0x02
#define DDSW_NOBPP              0x04
#define DDSW_NOVISIBLE          0x08
#define DDSW_NOFULLSCREEN       0x10
#define DDSW_NOCENTER           0x20
#define DDSW_NOCHANGES          (DDSW_NOSIZE & DDSW_NOMOVE & DDSW_NOBPP & \
                                 DDSW_NOFULLSCREEN & DDSW_NOVISIBLE & \
                                 DDSW_NOCENTER)
#endif

/**
 * Currently active window. There is always one active window, so no need
 * to worry about NULLs. The easiest way to get information about the
 * window where drawing is being is done.
 */
extern const Window* theWindow;

// A helpful macro that changes the origin of the screen
// coordinate system.
#define FLIP(y) (Window_Height(theWindow) - (y+1))

boolean Sys_InitWindowManager(void);
boolean Sys_ShutdownWindowManager(void);

boolean Sys_ChangeVideoMode(int width, int height, int bpp);
boolean Sys_GetDesktopBPP(int* bpp);

/**
 * Window management.
 */

/**
 * Constructs a new window using the default configuration. Note that the
 * default configuration is saved persistently when the engine shuts down and
 * is restored when the engine is restarted.
 *
 * Command line options (e.g., -xpos) can be used to modify the window
 * configuration.
 *
 * @param title  Text for the window title.
 *
 * @return  Window instance. Caller does not get ownership.
 *
 * @deprecated  Windows will be represented by CanvasWindow instances.
 */
Window* Window_New(const char* title);

#if 0
/**
 * Create a new window.
 *
 * @param app           Ptr to the application structure holding our globals.
 * @param origin        Origin of the window in desktop-space.
 * @param size          Size of the window (client area) in desktop-space.
 * @param bpp           BPP (bits-per-pixel)
 * @param flags         DDWF_* flags, control appearance/behavior.
 * @param type          Type of window to be created (WT_*)
 * @param title         Window title string, ELSE @c NULL,.
 * @param data          Platform specific data.
 *
 * @return              If @c 0, window creation was unsuccessful,
 *                      ELSE 1-based index identifier of the new window.
 *
 * @todo Refactor for New/Delete convention.
 */
uint Window_Create(application_t* app, const Point2Raw* origin,
                   const Size2Raw* size, int bpp, int flags, ddwindowtype_t type,
                   const char* title, void* data);
#endif

/**
 * Close and destroy the window. Its state is saved persistently and used as
 * the default configuration the next time the same window is created.
 *
 * @param wnd  Window instance.
 */
void Window_Delete(Window* wnd);

Window* Window_Main(void);

Window* Window_ByIndex(uint idx);

boolean Window_IsFullscreen(const Window* wnd);

boolean Window_IsCentered(const Window* wnd);

boolean Window_IsMaximized(const Window* wnd);

int Window_X(const Window* wnd);

int Window_Y(const Window* wnd);

/**
 * Returns the current width of the window.
 * @param wnd  Window instance.
 */
int Window_Width(const Window* wnd);

/**
 * Returns the current height of the window.
 * @param wnd  Window instance.
 */
int Window_Height(const Window* wnd);

int Window_NormalX(const Window* wnd);
int Window_NormalY(const Window* wnd);
int Window_NormalWidth(const Window* wnd);
int Window_NormalHeight(const Window* wnd);

/**
 * Determines the size of the window.
 * @param wnd  Window instance.
 * @return Window size.
 */
const Size2Raw* Window_Size(const Window* wnd);

int Window_ColorDepthBits(const Window* wnd);

/**
 * Sets the title of a window.
 *
 * @param win           Window instance.
 * @param title         New title for the window.
 */
void Window_SetTitle(const Window *win, const char* title);

void Window_Show(Window* wnd, boolean show);

/**
 * Sets or changes one or more window attributes.
 *
 * @param wnd      Window instance.
 * @param attribs  Array of values:
 *      <pre>[ attribId, value, attribId, value, ..., 0 ]</pre>
 *      The array must be zero-terminated, as that indicates where the array
 *      ends (see windowattribute_e).
 *
 * @return @c true, if the attributes were set and the window was successfully
 * updated. @c false, if there was an error with the values -- in this case all
 * the window's attributes remain unchanged.
 */
boolean Window_ChangeAttributes(Window* wnd, int* attribs);

/**
 * Request drawing the contents of the window as soon as possible.
 *
 * @param win  Window instance.
 */
void Window_Draw(Window* win);

/**
 * Make the content of the framebuffer visible.
 */
void Window_SwapBuffers(const Window* win);

/**
 * Grab the contents of the window into an OpenGL texture.
 *
 * @param win    Window instance.
 * @param halfSized  If @c true, scales the image to half the full size.
 *
 * @return OpenGL texture name. Caller is reponsible for deleting the texture.
 */
unsigned int Window_GrabAsTexture(const Window* win, boolean halfSized);

/**
 * Grabs the contents of the window and saves it into an image file.
 *
 * @param win       Window instance.
 * @param fileName  Name of the file to save. May include a file extension
 *                  that indicates which format to use (e.g, "screenshot.jpg").
 *                  If omitted, defaults to PNG.
 *
 * @return @c true if successful, otherwise @c false.
 */
boolean Window_GrabToFile(const Window* win, const char* fileName);

/**
 * Grab the contents of the window into an image.
 *
 * @param win    Window instance.
 * @param image  Grabbed image contents. Caller gets ownership; call GL_DestroyImage().
 */
void Window_Grab(const Window* win, image_t* image);

/**
 * Grab the contents of the window into an image.
 *
 * @param win    Window instance.
 * @param image  Grabbed image contents. Caller gets ownership; call GL_DestroyImage().
 * @param halfSized  If @c true, scales the image to half the full size.
 */
void Window_Grab2(const Window* win, image_t* image, boolean halfSized);

/**
 * Saves the window's state into a persistent storage so that it can be later
 * on restored. Used at shutdown time to save window geometry.
 *
 * @param wnd  Window instance.
 */
void Window_SaveState(Window *wnd);

/**
 * Restores the window's state from persistent storage. Used at engine startup
 * to determine the default window geometry.
 *
 * @param wnd  Window instance.
 */
void Window_RestoreState(Window* wnd);

/**
 * Activates or deactivates the window mouse trap. When trapped, the mouse cursor is
 * not visible and all mouse motions are interpreted as deltas.
 *
 * @param wnd     Window instance.
 * @param enable  @c true, if the mouse is to be trapped in the window.
 *
 * @note This is a C wrapper for Canvas::trapMouse().
 */
void Window_TrapMouse(const Window* wnd, boolean enable);

boolean Window_IsMouseTrapped(const Window* wnd);

/**
 * Determines whether the contents of a window should be drawn during the
 * execution of the main loop callback, or should we wait for an update event
 * from the windowing system.
 *
 * @param wnd  Window instance.
 */
boolean Window_ShouldRepaintManually(const Window* wnd);

void Window_UpdateCanvasFormat(Window* wnd);

/**
 * Activates the window's GL context so that OpenGL API calls can be made.
 * The GL context is automatically active during the drawing of the window's
 * contents; at other times it needs to be manually activated.
 *
 * @param wnd  Window instance.
 */
void Window_GLActivate(Window* wnd);

/**
 * Dectivates the window's GL context after OpenGL API calls have been done.
 * The GL context is automatically deactived after the drawing of the window's
 * contents; at other times it needs to be manually deactivated.
 *
 * @param wnd  Window instance.
 */
void Window_GLDone(Window* wnd);

void* Window_NativeHandle(const Window* wnd);

/**
 * Returns the window's native widget, if one exists.
 */
QWidget* Window_Widget(Window* wnd);

CanvasWindow *Window_CanvasWindow(Window *wnd);

/**
 * Utility to call after changing the size of a CanvasWindow. This will update
 * the Window state.
 *
 * @param win  Window instance.
 *
 * @deprecated In the future, size management will be done internally in
 * CanvasWindow/WindowSystem.
 */
void Window_UpdateAfterResize(Window *win);

#endif /* LIBDENG_SYS_WINDOW_H */
