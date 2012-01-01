/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * rend_model.h: 3D Models
 */

#ifndef __DOOMSDAY_RENDER_MODEL_H__
#define __DOOMSDAY_RENDER_MODEL_H__

typedef struct rendmodelparams_s {
// Animation, frame interpolation.
    struct modeldef_s *mf, *nextMF;
    float           inter;
    boolean         alwaysInterpolate;
    int             id; // For a unique skin offset.
    int             selector;

// Position/Orientation/Scale
    float           center[3], gzt; // The real center point and global top z for silhouette clipping.
    float           srvo[3]; // Short-range visual offset.
    float           distance; // Distance from viewer.
    float           yaw, extraYawAngle, yawAngleOffset; // \todo we don't need three sets of angles, update users of this struct instead.
    float           pitch, extraPitchAngle, pitchAngleOffset;

    float           extraScale;

    boolean         viewAlign;
    boolean         mirror; // If true the model will be mirrored about its Z axis (in model space).

// Appearance
    int             flags; // Mobj flags.
    int             tmap;

    // Lighting/color:
    float           ambientColor[4];
    uint            vLightListIdx;

    // Shinemaping:
    float           shineYawOffset;
    float           shinePitchOffset;
    boolean         shineTranslateWithViewerPos;
    boolean         shinepspriteCoordSpace; // Use the psprite coordinate space hack.
} rendmodelparams_t;

extern int      modelLight;
extern int      frameInter;
extern int      mirrorHudModels;
extern int      modelShinyMultitex;
extern float    rendModelLOD;

void            Rend_ModelRegister(void);
void            Rend_RenderModel(const rendmodelparams_t *params);

#endif
