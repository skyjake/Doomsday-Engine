/** @file sys_opengl.h
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * OpenGL interface, low-level.
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

#ifndef DE_SYSTEM_OPENGL_H
#define DE_SYSTEM_OPENGL_H

#ifdef __SERVER__
#  define GL_CLAMP_TO_EDGE    0
#endif

#ifdef __CLIENT__
#  include <de/opengl.h>
#  ifdef WIN32
#    define GL_CALL __stdcall
#  else
#    define GL_CALL
#  endif
#endif // __CLIENT__

//#ifndef GL_NV_texture_env_combine4
//#  define GL_NV_texture_env_combine4    1
//#  define GL_COMBINE4_NV                0x8503
//#  define GL_SOURCE3_RGB_NV             0x8583
//#  define GL_SOURCE3_ALPHA_NV           0x858B
//#  define GL_OPERAND3_RGB_NV            0x8593
//#  define GL_OPERAND3_ALPHA_NV          0x859B
//#endif

#include <string.h>

#ifdef __CLIENT__
#  include "gl_deferredapi.h"
#  include "ui/clientwindow.h"
#endif

/**
 * Configure available features
 * \todo Move out of this header.
 */
#define USE_TEXTURE_COMPRESSION_S3      1

/**
 * High-level GL state information.
 */
typedef struct gl_state_s {
    /// Current state:
    float currentLineWidth;
    float currentPointSize;

    /// Feature (abstract) availability bits:
    /// Vendor and implementation agnostic.
    struct {
        uint texCompression : 1;
        uint texFilterAniso : 1;
    } features;
} gl_state_t;

typedef enum arraytype_e {
    AR_VERTEX,
    AR_COLOR,
    AR_TEXCOORD0,
    AR_TEXCOORD1,
    AR_TEXCOORD2,
    AR_TEXCOORD3,
    AR_TEXCOORD4,
    AR_TEXCOORD5,
    AR_TEXCOORD6,
    AR_TEXCOORD7
} arraytype_t;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __CLIENT__

extern gl_state_t GL_state;

//#ifndef GL_ATI_texture_env_combine3
//#define GL_MODULATE_ADD_ATI             0x8744
//#define GL_MODULATE_SIGNED_ADD_ATI      0x8745
//#define GL_MODULATE_SUBTRACT_ATI        0x8746
//#endif

//#ifndef GL_ATI_texture_env_combine3
//#define GL_ATI_texture_env_combine3     1
//#endif

dd_bool Sys_GLPreInit(void);

/**
 * Initializes our OpenGL interface. Called once during engine statup.
 */
dd_bool Sys_GLInitialize(void);

/**
 * Close our OpenGL interface for good. Called once during engine shutdown.
 */
void Sys_GLShutdown(void);

/**
 * Configure the core features of OpenGL. Extensions are not configured here.
 */
void Sys_GLConfigureDefaultState(void);

/**
 * Echo the full list of available GL extensions to the console.
 */
void Sys_GLPrintExtensions(void);

dd_bool Sys_GLCheckErrorArgs(const char *file, int line);

#endif // __CLIENT__

#ifdef __cplusplus
} // extern "C"

/**
 * Information about the OpenGL driver and its capabilities.
 *
 * @return Styled text.
 */
de::String Sys_GLDescription();

#endif

#endif /* DE_SYSTEM_OPENGL_H */
