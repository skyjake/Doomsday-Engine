/** @file r_main.cpp
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>
#include <cstring>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_resource.h"
#include "de_network.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"
#include "de_ui.h"

#ifdef __CLIENT__
#  include "clientapp.h"
#endif

#include "gl/svg.h"

using namespace de;

int validCount = 1; // Increment every time a check is made.

// Precalculated math tables.
fixed_t *fineCosine = &finesine[FINEANGLES / 4];

float frameTimePos; // 0...1: fractional part for sharp game tics.

byte precacheSkins = true;
byte precacheMapMaterials = true;
byte precacheSprites = true;
byte texGammaLut[256];

int psp3d;
float pspLightLevelMultiplier = 1;
float pspOffset[2];
int levelFullBright;
int weaponOffsetScaleY = 1000;

/*
 * Console variables:
 */
float weaponFOVShift    = 45;
float weaponOffsetScale = 0.3183f; // 1/Pi
byte weaponScaleMode    = SCALEMODE_SMART_STRETCH;

void R_BuildTexGammaLut()
{
#ifdef __SERVER__
    double invGamma = 1.0f;
#else
    double invGamma = 1.0f - MINMAX_OF(0, texGamma, 1); // Clamp to a sane range.
#endif

    for(int i = 0; i < 256; ++i)
    {
        texGammaLut[i] = byte(255.0f * pow(double(i / 255.0f), invGamma));
    }
}

void R_Init()
{
    R_InitColorPalettes();
    R_InitTranslationTables();
    R_InitRawTexs();
    R_InitSvgs();
#ifdef __CLIENT__
    Viewports_Init();
#endif
}

void R_Update()
{
    // Reset file IDs so previously seen files can be processed again.
    F_ResetFileIds();
    // Re-read definitions.
    Def_Read();

    R_UpdateRawTexs();
    R_InitSprites(); // Fully reinitialize sprites.
    Models_Init(); // Defs might've changed.

    R_UpdateTranslationTables();

    Def_PostInit();

    App_World().update();

#ifdef __CLIENT__
    // Recalculate the light range mod matrix.
    Rend_UpdateLightModMatrix();

    // The rendering lists have persistent data that has changed during the
    // re-initialization.
    ClientApp::renderSystem().clearDrawLists();

    /// @todo fixme: Update the game title and the status.
#endif

#ifdef DENG_DEBUG
    Z_CheckHeap();
#endif
}

void R_Shutdown()
{
#ifdef __CLIENT__
    LensFx_Shutdown();
    Viewports_Shutdown();
#endif
    R_ClearAnimGroups();
    R_ShutdownSprites();
    Models_Shutdown();
    R_ShutdownSvgs();
}
