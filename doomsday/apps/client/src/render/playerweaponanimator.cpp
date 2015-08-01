/** @file playerweaponanimator.cpp  Player weapon animator.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/playerweaponanimator.h"
#include "render/mobjanimator.h"
#include "clientplayer.h"
#include "clientapp.h"

using namespace de;

DENG2_PIMPL_NOREF(PlayerWeaponAnimator)
{
    ClientPlayer *player;
    std::unique_ptr<MobjAnimator> animator;
    Matrix4f transform;
    gl::Cull cullFace;

    Instance(ClientPlayer *plr)
        : player(plr)
    {}

    void setupAsset(String const &identifier)
    {
        // Is there a model for the weapon?
        if(modelBank().has(identifier))
        {
            // Prepare the animation state of the model.
            ModelBank::ModelWithData loaded = modelBank().modelAndData(identifier);
            ModelDrawable &model = *loaded.first;
            animator.reset(new MobjAnimator(identifier, model));

            // The basic transformation of the model.
            auto const &aux = loaded.second->as<ModelRenderer::AuxiliaryData>();
            cullFace  = aux.cull;
            transform = aux.transformation;
        }
        else
        {
            animator.reset();
        }
    }

    static ModelBank &modelBank()
    {
        return ClientApp::renderSystem().modelRenderer().bank();
    }
};

PlayerWeaponAnimator::PlayerWeaponAnimator(ClientPlayer *plr)
    : d(new Instance(plr))
{}

void PlayerWeaponAnimator::setAsset(String const &identifier)
{
    d->setupAsset(identifier);
}

void PlayerWeaponAnimator::stateChanged(state_t const *state)
{
    if(d->animator)
    {
        d->animator->triggerByState(Def_GetStateName(state));
    }
}

MobjAnimator &PlayerWeaponAnimator::animator()
{
    DENG2_ASSERT(hasModel());
    return *d->animator;
}

void PlayerWeaponAnimator::setupVisPSprite(vispsprite_t &spr) const
{
    DENG2_ASSERT(hasModel());

    spr.type = VPSPR_MODEL2;
    spr.data.model2.model = model();
    spr.data.model2.animator = d->animator.get();
    spr.data.model2.cullFace = d->cullFace;
    ByteRefArray(spr.data.model2.modelTransform, sizeof(Matrix4f))
            .set(0, (dbyte const *) d->transform.values(), sizeof(Matrix4f));
}

void PlayerWeaponAnimator::advanceTime(const TimeDelta &elapsed)
{
    if(d->animator)
    {
        d->animator->advanceTime(elapsed);
    }
}

bool PlayerWeaponAnimator::hasModel() const
{
    return bool(d->animator);
}

ModelDrawable const *PlayerWeaponAnimator::model() const
{
    if(!hasModel()) return nullptr;
    return &d->animator->model();
}
