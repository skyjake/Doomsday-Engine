/** @file controllerpresets.h  Game controller presets (bindings).
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_UI_CONTROLLERPRESETS_H
#define DE_CLIENT_UI_CONTROLLERPRESETS_H

#include <de/string.h>

/**
 * Game controller presets.
 */
class ControllerPresets
{
public:
    ControllerPresets(int deviceId, const char *presetCVarPath);

    de::String currentPreset() const;

    de::StringList ids() const;

    void applyPreset(const de::String &presetId);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_UI_CONTROLLERPRESETS_H

