/** @file render/vr.h  Stereoscopic rendering and Oculus Rift support.
 *
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_RENDER_VR_H
#define CLIENT_RENDER_VR_H

#include "dd_types.h"
#include <de/vrconfig.h>

de::VRConfig &vrCfg();

namespace VR
{
    /// (UNUSED) Distance from player character to weapon sprite, in map units
    extern float weaponDistance;
}

/**
 * Register VR console variables.
 */
void VR_ConsoleRegister();

/**
 * Returns the horizontal field of view in Oculus Rift in degrees.
 */
//float VR_RiftFovX();

/**
 * Load Oculus Rift parameters via Rift SDK.
 */
//bool VR_LoadRiftParameters();

#endif // CLIENT_RENDER_VR_H
