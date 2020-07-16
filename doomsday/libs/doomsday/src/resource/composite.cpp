/** @file compositetexture.cpp  Composite Texture.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/res/composite.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/filesys/lumpindex.h"
#include "doomsday/res/patch.h"
#include "doomsday/res/patchname.h"

#include <de/list.h>
#include <de/rectangle.h>
#include <de/byterefarray.h>
#include <de/string.h>
#include <de/reader.h>

using namespace de;

namespace res {

namespace internal {

static String readAndPercentEncodeRawName(de::Reader &from)
{
    /// @attention The raw ASCII name is not necessarily terminated.
    char asciiName[9];
    for (int i = 0; i < 8; ++i) { from >> asciiName[i]; }
    asciiName[8] = 0;

    // WAD format allows characters not typically permitted in native paths.
    // To achieve uniformity we apply a percent encoding to the "raw" names.
    return String(asciiName).toPercentEncoding();
}

} // namespace internal

Composite::Component::Component(const Vec2i &origin)
    : _origin (origin)
    , _lumpNum(-1)
{}

void Composite::Component::setOrigin(const Vec2i &origin)
{
    _origin = origin;
}

bool Composite::Component::operator == (const Component &other) const
{
    if (lumpNum() != other.lumpNum()) return false;
    if (origin()  != other.origin())  return false;
    return true;
}

bool Composite::Component::operator != (const Component &other) const
{
    return !(*this == other);
}

const Vec2i &Composite::Component::origin() const
{
    return _origin;
}

lumpnum_t Composite::Component::lumpNum() const
{
    return _lumpNum;
}

void Composite::Component::setLumpNum(lumpnum_t num)
{
    _lumpNum = num;
}

DE_PIMPL_NOREF(Composite)
{
    String name;                 ///< Symbolic, percent encoded.
    Flags flags;                 ///< Usage traits.
    Vec2ui logicalDimensions; ///< In map space units.
    Vec2ui dimensions;        ///< In pixels.
    int origIndex;               ///< Determined by the original game logic.
    Components components;       ///< Images to be composited.

    Impl() : origIndex(-1) {}
};

Composite::Composite(const String &percentEncodedName,
    const Vec2ui &logicalDimensions, Flags flags)
    : d(new Impl)
{
    d->name              = percentEncodedName;
    d->flags             = flags;
    d->logicalDimensions = logicalDimensions;
}

bool Composite::operator == (const Composite &other) const
{
    if (dimensions()        != other.dimensions())        return false;
    if (logicalDimensions() != other.logicalDimensions()) return false;
    if (componentCount()    != other.componentCount())    return false;

    // Check each component also.
    for (int i = 0; i < componentCount(); ++i)
    {
        if (components()[i] != other.components()[i]) return false;
    }

    return true;
}

String Composite::percentEncodedName() const
{
    return d->name;
}

const String &Composite::percentEncodedNameRef() const
{
    return d->name;
}

const Vec2ui &Composite::logicalDimensions() const
{
    return d->logicalDimensions;
}

const Vec2ui &Composite::dimensions() const
{
    return d->dimensions;
}

const Composite::Components &Composite::components() const
{
    return d->components;
}

Flags Composite::flags() const
{
    return d->flags;
}

void Composite::setFlags(Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

int Composite::origIndex() const
{
    return d->origIndex;
}

void Composite::setOrigIndex(int newIndex)
{
    d->origIndex = newIndex;
}

Composite *Composite::constructFrom(de::Reader &reader,
                                    List<PatchName> const &patchNames,
                                    ArchiveFormat format)
{
    Composite *pctex = new Composite;

    // First is the raw name.
    pctex->d->name = res::internal::readAndPercentEncodeRawName(reader);

    // Next is some unused junk from a previous format version.
    dint16 unused16;
    reader >> unused16;

    // Next up are scale and logical dimensions.
    byte scale[2]; /** @todo ZDoom defines these otherwise unused bytes as a
                       scale factor (div 8). We could interpret this also. */
    dint16 dimensions[2];

    reader >> scale[0] >> scale[1] >> dimensions[0] >> dimensions[1];

    // We'll initially accept these values as logical dimensions. However
    // we may need to adjust once we've checked the patch dimensions.
    pctex->d->logicalDimensions =
        pctex->d->dimensions = Vec2ui(dimensions[0], dimensions[1]);

    if (format == DoomFormat)
    {
        // Next is some more unused junk from a previous format version.
        dint32 unused32;
        reader >> unused32;
    }

    /*
     * Finally, read the component images.
     * In the process we'll determine the final logical dimensions of the
     * texture by compositing the geometry of the component images.
     */
    dint16 componentCount;
    reader >> componentCount;

    Rectanglei geom(Vec2i(0, 0),
                    Vec2i(pctex->d->logicalDimensions.x, pctex->d->logicalDimensions.y));

    int foundComponentCount = 0;
    for (dint16 i = 0; i < componentCount; ++i)
    {
        Component comp;

        dint16 origin16[2];
        reader >> origin16[0] >> origin16[1];
        comp.setOrigin(Vec2i(origin16[0], origin16[1]));

        dint16 pnamesIndex;
        reader >> pnamesIndex;

        if (pnamesIndex < 0 || pnamesIndex >= patchNames.count())
        {
            LOG_RES_WARNING("Invalid PNAMES index %i in composite texture \"%s\", ignoring.")
                    << pnamesIndex << pctex->d->name;
        }
        else
        {
            comp.setLumpNum(patchNames[pnamesIndex].lumpNum());

            if (comp.lumpNum() >= 0)
            {
                /// There is now one more found component.
                foundComponentCount += 1;

                File1 &file = App_FileSystem().lump(comp.lumpNum());

                // If this a "custom" component - the whole texture is.
                if (file.container().hasCustom())
                {
                    pctex->d->flags |= Custom;
                }

                // If this is a Patch - unite the geometry of the component.
                ByteRefArray fileData{file.cache(), file.size()};
                if (res::Patch::recognize(fileData))
                {
                    try
                    {
                        auto info = res::Patch::loadMetadata(fileData);
                        geom |= Rectanglei::fromSize(Vec2i(comp.origin().x, comp.origin().y),
                                                     Vec2ui(info.dimensions.x, info.dimensions.y));
                    }
                    catch (const IByteArray::OffsetError &)
                    {
                        LOG_RES_WARNING("Component image \"%s\" (#%i) does not appear to be a valid Patch. "
                                        "It may be missing from composite texture \"%s\".")
                                << patchNames[pnamesIndex].percentEncodedNameRef() << i
                                << pctex->d->name;
                    }
                }
                file.unlock();
            }
            else
            {
                LOG_RES_WARNING("Missing component image \"%s\" (#%i) in composite texture \"%s\", ignoring.")
                        << patchNames[pnamesIndex].percentEncodedNameRef() << i
                        << pctex->d->name;
            }
        }

        // Skip the unused "step dir" and "color map" values.
        reader >> unused16 >> unused16;

        // Add this component.
        pctex->d->components.push_back(comp);
    }

    // Clip and apply the final height.
    if (geom.top() < 0) geom.topLeft.y = 0;
    if (geom.height() > pctex->d->logicalDimensions.y)
    {
        pctex->d->dimensions.y = geom.height();
    }

    if (!foundComponentCount)
    {
        LOG_RES_WARNING("Zero valid component images in composite texture %s (will be ignored).")
            << pctex->d->name;
    }

    return pctex;
}

} // namespace res
