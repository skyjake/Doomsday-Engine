/**
 * @file patchcompositetexture.cpp Patch Composite Texture Definition.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <QList>
#include <de/String>
#include <de/Reader>
#include "resource/patchname.h"
#include "resource/patchcompositetexture.h"

namespace de {

PatchCompositeTexture::Patch::Patch(int xOrigin, int yOrigin,
    int _pnamesIndex)
    : origin(xOrigin, yOrigin), pnamesIndex(_pnamesIndex), lumpNum(-1)
{}

void PatchCompositeTexture::Patch::operator << (Reader &from)
{
    dint16 origin16[2];
    from >> origin16[0] >> origin16[1];
    origin.x = origin16[0];
    origin.y = origin16[1];

    dint16 pnamesIndex16;
    from >> pnamesIndex16;
    pnamesIndex = pnamesIndex16;

    // Skip the unused "step dir" and "color map" values.
    dint16 unused;
    from >> unused >> unused;
}

PatchCompositeTexture::PatchCompositeTexture(String percentEncodedName, int width,
    int height, Flags _flags)
    : name(percentEncodedName), flags_(_flags), dimensions_(width, height)
{}

void PatchCompositeTexture::validate(QList<PatchName> patchNames)
{
    int foundPatchCount = 0;
    DENG2_FOR_EACH(Patches, i, patches_)
    {
        Patch &patchDef = *i;

        if(patchDef.pnamesIndex < 0 || patchDef.pnamesIndex >= patchNames.count())
        {
            LOG_WARNING("Invalid PNAMES index %i in definition \"%s\".")
                << patchDef.pnamesIndex << name;
        }
        else
        {
            patchDef.lumpNum = patchNames[patchDef.pnamesIndex].lumpNum();
        }

        if(patchDef.lumpNum >= 0)
            foundPatchCount += 1;
    }

    if(!foundPatchCount)
    {
        LOG_WARNING("Zero valid patches in texture definition \"%s\".") << name;
    }
}

static String readAndPercentEncodeRawName(Reader &from)
{
    /// @attention The raw ASCII name is not necessarily terminated.
    char asciiName[9];
    for(int i = 0; i < 8; ++i) { from >> asciiName[i]; }
    asciiName[8] = 0;

    // WAD format allows characters not typically permitted in native paths.
    // To achieve uniformity we apply a percent encoding to the "raw" names.
    return QString(QByteArray(asciiName).toPercentEncoding());
}

void PatchCompositeTexture::operator << (Reader &from)
{
    // First is the raw name.
    name = readAndPercentEncodeRawName(from);

    // Next is some unused junk from a previous format version.
    dint16 unused16;
    from >> unused16;

    // Next up are scale and logical dimensions.
    byte scale[2]; // [x, y] Used by ZDoom (div 8), presently unused.
    dint16 dimensions[2];

    from >> scale[0] >> scale[1] >> dimensions[0] >> dimensions[1];

    dimensions_.width  = dimensions[0];
    dimensions_.height = dimensions[1];

    // Next is some more unused junk from a previous format version.
    dint32 unused32;
    from >> unused32;

    // Finally, read the patches.
    dint16 patchCount;
    from >> patchCount;
    for(dint16 i = 0; i < patchCount; ++i)
    {
        Patch patch;
        from >> patch;
        patches_.push_back(patch);
    }
}

PatchCompositeTexture PatchCompositeTexture::fromDoomFormat(Reader &from)
{
    PatchCompositeTexture pctex;
    from >> pctex;
    return pctex;
}

PatchCompositeTexture PatchCompositeTexture::fromStrifeFormat(Reader &from)
{
    PatchCompositeTexture pctex;

    // First is the raw name.
    pctex.name = readAndPercentEncodeRawName(from);

    // Next is some unused junk from a previous format version.
    dint16 unused16;
    from >> unused16;

    // Next up are scale and logical dimensions.
    byte scale[2]; // [x, y] Used by ZDoom (div 8), presently unused.
    dint16 dimensions[2];

    from >> scale[0] >> scale[1] >> dimensions[0] >> dimensions[1];

    pctex.dimensions_.width  = dimensions[0];
    pctex.dimensions_.height = dimensions[1];

    // Finally, read the patches.
    dint16 patchCount;
    from >> patchCount;
    for(dint16 i = 0; i < patchCount; ++i)
    {
        Patch patch;
        from >> patch;
        pctex.patches_.push_back(patch);
    }

    return pctex;
}

String PatchCompositeTexture::percentEncodedName() const {
    return name;
}

String const &PatchCompositeTexture::percentEncodedNameRef() const {
    return name;
}

Size2Raw const &PatchCompositeTexture::dimensions() const {
    return dimensions_;
}

PatchCompositeTexture::Patches const &PatchCompositeTexture::patches() const {
    return patches_;
}

PatchCompositeTexture::Flags const &PatchCompositeTexture::flags() const {
    return flags_;
}

int PatchCompositeTexture::origIndex() const {
    return origIndex_;
}

} // namespace de
