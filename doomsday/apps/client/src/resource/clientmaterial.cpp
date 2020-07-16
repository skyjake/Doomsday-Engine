/** @file clientmaterial.cpp  Client-side material.
 *
 * @authors Copyright © 2009-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "resource/clientmaterial.h"
#include "dd_main.h"
#include "resource/materialanimator.h"

#include <doomsday/console/cmd.h>
#include <doomsday/res/textures.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/materialmanifest.h>
#include <de/log.h>
#include "resource/materialanimator.h"

using namespace de;

DE_PIMPL_NOREF(ClientMaterial::Decoration)
{
    ClientMaterial *material = nullptr; ///< Owning Material.
    Vec2i           patternSkip;        ///< Pattern skip intervals.
    Vec2i           patternOffset;      ///< Pattern skip interval offsets.
};

ClientMaterial::Decoration::Decoration(const Vec2i &patternSkip, const Vec2i &patternOffset)
    : d(new Impl)
{
    d->patternSkip   = patternSkip;
    d->patternOffset = patternOffset;
}

ClientMaterial::Decoration::~Decoration()
{
    deleteAll(_stages);
}

ClientMaterial &ClientMaterial::Decoration::material()
{
    DE_ASSERT(d->material);
    return *d->material;
}

const ClientMaterial &ClientMaterial::Decoration::material() const
{
    DE_ASSERT(d->material);
    return *d->material;
}

void ClientMaterial::Decoration::setMaterial(ClientMaterial *newOwner)
{
    d->material = newOwner;
}

const Vec2i &ClientMaterial::Decoration::patternSkip() const
{
    return d->patternSkip;
}

const Vec2i &ClientMaterial::Decoration::patternOffset() const
{
    return d->patternOffset;
}

int ClientMaterial::Decoration::stageCount() const
{
    return _stages.count();
}

ClientMaterial::Decoration::Stage &ClientMaterial::Decoration::stage(int index) const
{
    if (!stageCount())
    {
        /// @throw MissingStageError  No stages are defined.
        throw MissingStageError("ClientMaterial::Decoration::stage", "Decoration has no stages");
    }
    return *_stages[de::wrap(index, 0, _stages.count())];
}

String ClientMaterial::Decoration::describe() const
{
    return "abstract Decoration";
}

String ClientMaterial::Decoration::description() const
{
    const int numStages = stageCount();
    String    str       = _E(b) + describe() + _E(.) + " (" + String::asText(numStages) + " stage" +
                          DE_PLURAL_S(numStages) + "):";
    for (int i = 0; i < numStages; ++i)
    {
        str += Stringf("\n  [%2i] ", i) + _E(>) + stage(i).description() + _E(<);
    }
    return str;
}

DE_PIMPL(ClientMaterial)
{
    /// Decorations (owned), to be projected into the world (relative to a Surface).
    List<Decoration *> decorations;

    /// Set of draw-context animators (owned).
    List<MaterialAnimator *> animators;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        self().clearAllAnimators();
        self().clearAllDecorations();
    }

    MaterialAnimator *findAnimator(const MaterialVariantSpec &spec, bool canCreate = false)
    {
        for (auto *iter : animators)
        {
            if (iter->variantSpec().compare(spec))
            {
                return const_cast<MaterialAnimator *>(iter);  // This will do fine.
            }
        }

        if (!canCreate) return nullptr;

        animators.append(new MaterialAnimator(self(), spec));
        return animators.back();
    }
};

ClientMaterial::ClientMaterial(world::MaterialManifest &manifest)
    : world::Material(manifest)
    , d(new Impl(this))
{}

ClientMaterial::~ClientMaterial()
{}

bool ClientMaterial::isAnimated() const
{
    if (Material::isAnimated())
    {
        return true;
    }
    for (const auto &decor : d->decorations)
    {
        if (decor->isAnimated())
        {
            return true;
        }
    }
    return false;
}

int ClientMaterial::decorationCount() const
{
    return d->decorations.count();
}

LoopResult ClientMaterial::forAllDecorations(const std::function<LoopResult (Decoration &)> &func) const
{
    for (Decoration *decor : d.getConst()->decorations)
    {
        if (auto result = func(*decor)) return result;
    }
    return LoopContinue;
}

/// @todo Update client side MaterialAnimators?
void ClientMaterial::addDecoration(Decoration *decor)
{
    if (!decor || d->decorations.contains(decor)) return;

    decor->setMaterial(this);
    d->decorations.append(decor);
}

void ClientMaterial::clearAllDecorations()
{
    deleteAll(d->decorations); d->decorations.clear();

    // Animators refer to decorations.
    clearAllAnimators();
}

int ClientMaterial::animatorCount() const
{
    return d.getConst()->animators.count();
}

MaterialAnimator &ClientMaterial::animator(int index) const
{
    return *d.getConst()->animators[index];
}

bool ClientMaterial::hasAnimator(const MaterialVariantSpec &spec)
{
    return d->findAnimator(spec) != nullptr;
}

MaterialAnimator &ClientMaterial::getAnimator(const MaterialVariantSpec &spec)
{
    return *d->findAnimator(spec, true/*create*/);
}

LoopResult ClientMaterial::forAllAnimators(const std::function<LoopResult (MaterialAnimator &)> &func) const
{
    for (MaterialAnimator *animator : d.getConst()->animators)
    {
        if (auto result = func(*animator)) return result;
    }
    return LoopContinue;
}

void ClientMaterial::clearAllAnimators()
{
    deleteAll(d->animators);
    d->animators.clear();
}

ClientMaterial &ClientMaterial::find(const res::Uri &uri) // static
{
    return world::Materials::get().material(uri).as<ClientMaterial>();
}

String ClientMaterial::description() const
{
    String str = world::Material::description();

    str += _E(b) " x" + String::asText(animatorCount()) + _E(.) + _E(l) " EnvClass: \"" _E(.) +
           (audioEnvironment() == AE_NONE ? "N/A" : S_AudioEnvironmentName(audioEnvironment())) +
           "\"";

    // Add the decoration config:
    for (const auto *decor : d->decorations)
    {
        str += "\n" + decor->description();
    }

    return str;
}
