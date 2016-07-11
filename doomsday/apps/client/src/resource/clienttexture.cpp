/** @file texture.cpp  Logical texture resource.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "resource/clienttexture.h"
#include "resource/clientresources.h"

#include <doomsday/console/cmd.h>
#include <doomsday/res/TextureManifest>
#include <de/Error>
#include <de/Log>
#include <de/memory.h>
#include <QMap>

using namespace de;

DENG2_PIMPL(ClientTexture)
{
    /// Set of (render-) context variants.
    Variants variants;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        clearVariants();
    }

    void clearVariants()
    {
        while (!variants.isEmpty())
        {
            ClientTexture::Variant *variant = variants.takeFirst();
#ifdef DENG_DEBUG
            if (variant->glName())
            {
                String textualVariantSpec = variant->spec().asText();

                LOG_AS("ClientTexture::clearVariants")
                LOGDEV_RES_WARNING("GLName (%i) still set for a variant of \"%s\" %p. Perhaps it wasn't released?")
                    << variant->glName()
                    << self.manifest().composeUri()
                    << this
                    << textualVariantSpec;
            }
#endif
            delete variant;
        }
    }
};

ClientTexture::ClientTexture(res::TextureManifest &manifest)
    : res::Texture(manifest)
    , d(new Impl(this))
{
    setFlags(manifest.flags());
    setDimensions(manifest.logicalDimensions());
    setOrigin(manifest.origin());
}

duint ClientTexture::variantCount() const
{
    return duint(d->variants.count());
}

ClientTexture::Variant *ClientTexture::chooseVariant(ChooseVariantMethod method,
                                                     TextureVariantSpec const &spec,
                                                     bool canCreate)
{
    foreach (Variant *variant, d->variants)
    {
        TextureVariantSpec const &cand = variant->spec();
        switch (method)
        {
        case MatchSpec:
            if (cand == spec)
            {
                // This is the one we're looking for.
                return variant;
            }
            break;

        /*case FuzzyMatchSpec:
            if (cand == spec)
            {
                // This will do fine.
                return variant;
            }
            break;*/
        }
    }

    /*
#ifdef DENG_DEBUG
    // 07/04/2011 dj: The "fuzzy selection" features are yet to be implemented.
    // As such, the following should NOT return a valid variant iff the rest of
    // this subsystem has been implemented correctly.
    //
    // Presently this is used as a sanity check.
    if (method == MatchSpec)
    {
        DENG_ASSERT(!chooseVariant(FuzzyMatchSpec, spec));
    }
#endif
     */

    if (!canCreate) return 0;

    d->variants.push_back(new Variant(*this, spec));
    return d->variants.back();
}

ClientTexture::Variant *ClientTexture::prepareVariant(TextureVariantSpec const &spec)
{
    Variant *variant = chooseVariant(MatchSpec, spec, true /*can create*/);
    DENG2_ASSERT(variant);
    variant->prepare();
    return variant;
}

ClientTexture::Variants const &ClientTexture::variants() const
{
    return d->variants;
}

void ClientTexture::clearVariants()
{
    d->clearVariants();
}

void ClientTexture::release(/*TextureVariantSpec *spec*/)
{
    Texture::release();
    
    foreach (TextureVariant *variant, d->variants)
    {
        //if (!spec || spec == &variant->spec())
        {
            variant->release();
            /*if (spec)
            {
                break;
            }*/
        }
    }
}

String ClientTexture::description() const
{
    String variantDesc;
    QTextStream os(&variantDesc);

    if (variantCount())
    {
        // Print variant specs.
        os << "\n" << _E(R);

        //foreach (TextureVariant *variant, texture.variants())
        for (int variantIdx = 0; variantIdx < d->variants.size(); ++variantIdx)
        {
            auto const *variant = d->variants.at(variantIdx);

            Vector2f coords;
            variant->glCoords(&coords.x, &coords.y);

            String textualVariantSpec = variant->spec().asText();

            os << "\n"
               << _E(D) "Variant #" << variantIdx << ":" _E(.)
               << " " _E(l) "Source: " _E(.) _E(i) << variant->sourceDescription() << _E(.)
               << " " _E(l) "Masked: " _E(.) _E(i) << (variant->isMasked()? "yes":"no") << _E(.)
               << " " _E(l) "GLName: " _E(.) _E(i) << variant->glName() << _E(.)
               << " " _E(l) "Coords: " _E(.) _E(i) << coords.asText() << _E(.)
               << _E(R)
               << "\n" _E(b) "Specification:" _E(.)
               << textualVariantSpec;

            ++variantIdx;
        }
    }

    return res::Texture::description() +
           String(" x%1").arg(variantCount()) +
           variantDesc;
}

