/** @file g_defs.cpp  Game definition lookup utilities.
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "g_defs.h"

#include <de/recordvalue.h>
#include <doomsday/defs/episode.h>

using namespace de;

ded_t &Defs()
{
    return *static_cast<ded_t *>(DD_GetVariable(DD_DEFS));
}

dint PlayableEpisodeCount()
{
    dint count = 0;
    const DictionaryValue::Elements &episodesById = Defs().episodes.lookup("id").elements();
    for(const auto &pair : episodesById)
    {
        const Record &episodeDef = *pair.second->as<RecordValue>().record();
        res::Uri startMap(episodeDef.gets("startMap"), RC_NULL);
        if(P_MapExists(startMap.compose()))
        {
            count += 1;
        }
    }
    return count;
}

String FirstPlayableEpisodeId()
{
    const DictionaryValue::Elements &episodesById = Defs().episodes.lookup("id").elements();
    for(const auto &pair : episodesById)
    {
        const Record &episodeDef = *pair.second->as<RecordValue>().record();
        res::Uri startMap(episodeDef.gets("startMap"), RC_NULL);
        if(P_MapExists(startMap.compose()))
        {
            return episodeDef.gets("id");
        }
    }
    return "";  // Not found.
}

res::Uri TranslateMapWarpNumber(const String &episodeId, dint warpNumber)
{
    if(const Record *rec = Defs().episodes.tryFind("id", episodeId))
    {
        defn::Episode episodeDef(*rec);
        if(const Record *mgNodeRec = episodeDef.tryFindMapGraphNodeByWarpNumber(warpNumber))
        {
            return res::makeUri(mgNodeRec->gets("id"));
        }
    }
    return res::makeUri("Maps:");  // Not found.
}

