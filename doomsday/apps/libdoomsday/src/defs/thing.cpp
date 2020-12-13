/** @file thing.cpp  Thing definition.
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

#include "doomsday/defs/thing.h"
#include "doomsday/defs/ded.h"

#include <de/TextValue>

using namespace de;

namespace defn {
    
void Thing::resetToDefaults()
{
    Definition::resetToDefaults();
    
    // Add all expected fields with their default values.
    def().addText  (VAR_ID, "");
    def().addNumber("doomEdNum", 0);
    def().addText  ("name", "");
    def().addArray ("states").array().addMany(STATENAMES_COUNT, "");
    def().addArray ("sounds").array().addMany(SOUNDNAMES_COUNT, "");
    def().addNumber("reactionTime", 0);
    def().addNumber("painChance", 0);
    def().addNumber("spawnHealth", 0);
    def().addNumber("speed", 0);
    def().addNumber("radius", 0);
    def().addNumber("height", 0);
    def().addNumber("mass", 0);
    def().addNumber("damage", 0);
    def().addText  ("onTouch", ""); // script function to call when touching a special thing
    def().addText  ("onDeath", ""); // script function to call when thing is killed
    def().addArray ("flags").array().addMany(NUM_MOBJ_FLAGS, 0);
    def().addArray ("misc").array().addMany(NUM_MOBJ_MISC, 0);
}
    
void Thing::setSound(int soundId, String const &sound)
{
    def()["sounds"].array().setElement(soundId, sound);
}
    
String Thing::sound(int soundId) const
{
    return geta("sounds")[soundId].asText();
}

int Thing::flags(dint index) const
{
    return geta("flags")[index].asInt();
}
    
void Thing::setFlags(dint index, int flags)
{
    def()["flags"].array()
        .setElement(NumberValue(index), new NumberValue(flags, NumberValue::Hex));
}

} // namespace defn
