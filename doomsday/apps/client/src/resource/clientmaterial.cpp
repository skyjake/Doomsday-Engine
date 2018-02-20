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

#include <QFlag>
#include <QtAlgorithms>
#include <de/Log>
#include <doomsday/console/cmd.h>
#include <doomsday/res/Textures>
#include <doomsday/world/Materials>
#include <doomsday/world/MaterialManifest>

#include "dd_main.h"
#include "MaterialAnimator"

using namespace de;

DENG2_PIMPL_NOREF(ClientMaterial::Decoration)
{
    ClientMaterial *material = nullptr;  ///< Owning Material.
    Vec2i patternSkip;          ///< Pattern skip intervals.
    Vec2i patternOffset;        ///< Pattern skip interval offsets.
};

ClientMaterial::Decoration::Decoration(Vec2i const &patternSkip, Vec2i const &patternOffset)
    : d(new Impl)
{
    d->patternSkip   = patternSkip;
    d->patternOffset = patternOffset;
}

ClientMaterial::Decoration::~Decoration()
{
    qDeleteAll(_stages);
}

ClientMaterial &ClientMaterial::Decoration::material()
{
    DENG2_ASSERT(d->material);
    return *d->material;
}

ClientMaterial const &ClientMaterial::Decoration::material() const
{
    DENG2_ASSERT(d->material);
    return *d->material;
}

void ClientMaterial::Decoration::setMaterial(ClientMaterial *newOwner)
{
    d->material = newOwner;
}

Vec2i const &ClientMaterial::Decoration::patternSkip() const
{
    return d->patternSkip;
}

Vec2i const &ClientMaterial::Decoration::patternOffset() const
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
    int const numStages = stageCount();
    String str = _E(b) + describe() + _E(.) + " (" + String::number(numStages) + " stage" + DENG2_PLURAL_S(numStages) + "):";
    for (int i = 0; i < numStages; ++i)
    {
        str += String("\n  [%1] ").arg(i, 2) + _E(>) + stage(i).description() + _E(<);
    }
    return str;
}

DENG2_PIMPL(ClientMaterial)
{
    AudioEnvironmentId audioEnvironment { AE_NONE };

    /// Decorations (owned), to be projected into the world (relative to a Surface).
    QVector<Decoration *> decorations;

    /// Set of draw-context animators (owned).
    QVector<MaterialAnimator *> animators;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        self().clearAllAnimators();
        self().clearAllDecorations();
    }

    MaterialAnimator *findAnimator(MaterialVariantSpec const &spec, bool canCreate = false)
    {
        for (auto iter = animators.constBegin(); iter != animators.constEnd(); ++iter)
        {
            if ((*iter)->variantSpec().compare(spec))
            {
                return const_cast<MaterialAnimator *>(*iter);  // This will do fine.
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

AudioEnvironmentId ClientMaterial::audioEnvironment() const
{
    return (isDrawable()? d->audioEnvironment : AE_NONE);
}

void ClientMaterial::setAudioEnvironment(AudioEnvironmentId newEnvironment)
{
    d->audioEnvironment = newEnvironment;
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
    qDeleteAll(d->decorations); d->decorations.clear();

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

bool ClientMaterial::hasAnimator(MaterialVariantSpec const &spec)
{
    return d->findAnimator(spec) != nullptr;
}

MaterialAnimator &ClientMaterial::getAnimator(MaterialVariantSpec const &spec)
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
    qDeleteAll(d->animators);
    d->animators.clear();
}

ClientMaterial &ClientMaterial::find(de::Uri const &uri) // static
{
    return world::Materials::get().material(uri).as<ClientMaterial>();
}

String ClientMaterial::description() const
{
    String str = world::Material::description();

    str += _E(b) " x" + String::number(animatorCount()) + _E(.)
         + _E(l) " EnvClass: \"" _E(.) + (audioEnvironment() == AE_NONE? "N/A" : S_AudioEnvironmentName(audioEnvironment())) + "\"";

    // Add the decoration config:
    for (Decoration const *decor : d->decorations)
    {
        str += "\n" + decor->description();
    }

    return str;
}
