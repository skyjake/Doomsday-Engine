/** @file materialbind.cpp Material Bind.
 *
 * @author Copyright &copy; 2011-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include <de/memory.h>

#include "uri.hh"
#include "resource/materialbind.h"

namespace de {

MaterialBind &MaterialBind::setMaterial(material_t *newMaterial)
{
    if(asocMaterial != newMaterial)
    {
        // Any extended info will be invalid after this op, so destroy it
        // (it will automatically be rebuilt later, if subsequently needed).
        MaterialBindInfo *detachedInfo = detachInfo();
        if(detachedInfo) M_Free(detachedInfo);

        // Associate with the new Material.
        asocMaterial = newMaterial;
    }
    return *this;
}

MaterialBind &MaterialBind::attachInfo(MaterialBindInfo &info)
{
    LOG_AS("MaterialBind::attachInfo");
    if(extInfo)
    {
#if _DEBUG
        Uri uri = App_Materials()->composeUri(guid);
        LOG_DEBUG("Info already present for \"%s\", will replace.") << uri;
#endif
        M_Free(extInfo);
    }
    extInfo = &info;
    return *this;
}

MaterialBindInfo *MaterialBind::detachInfo()
{
    MaterialBindInfo *retInfo = extInfo;
    extInfo = 0;
    return retInfo;
}

ded_detailtexture_t *MaterialBind::detailTextureDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return 0;
    return extInfo->detailtextureDefs[Material_Prepared(asocMaterial)-1];
}

ded_decor_t *MaterialBind::decorationDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return 0;
    return extInfo->decorationDefs[Material_Prepared(asocMaterial)-1];
}

ded_ptcgen_t *MaterialBind::ptcGenDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return 0;
    return extInfo->ptcgenDefs[Material_Prepared(asocMaterial)-1];
}

ded_reflection_t *MaterialBind::reflectionDef() const
{
    if(!extInfo || !asocMaterial || !Material_Prepared(asocMaterial)) return 0;
    return extInfo->reflectionDefs[Material_Prepared(asocMaterial)-1];
}

} // namespace de
