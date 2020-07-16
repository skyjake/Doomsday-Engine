/** @file playerweaponanimator.cpp  Player weapon animator.
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

#include "render/playerweaponanimator.h"
#include "render/stateanimator.h"
#include "render/vissprite.h"
#include "clientplayer.h"
#include "clientapp.h"
#include "client/cl_def.h"
#include "def_main.h"
#include "render/r_main.h"
#include "render/rendersystem.h"

#include <de/legacy/timer.h>
#include <de/garbage.h>
#include <de/animationvector.h>

using namespace de;

namespace render {

DE_PIMPL_NOREF(PlayerWeaponAnimator)
, DE_OBSERVES(Asset, Deletion)
{
    ClientPlayer *player;
    String identifier;
    std::unique_ptr<StateAnimator> animator;
    AnimationVector2 angleOffset { Animation::Linear };

    Impl(ClientPlayer *plr)
        : player(plr)
    {}

    void setupAsset(const String &identifier)
    {
        this->identifier = identifier;
        angleOffset = Vec2f();

        if (animator)
        {
            animator->model().audienceForDeletion() -= this;
        }

        // Is there a model for the weapon?
        if (modelBank().has(identifier))
        {
            // Prepare the animation state of the model.
            auto &model = modelBank().model<Model>(identifier);
            model.audienceForDeletion() += this;
            animator.reset(new StateAnimator(identifier, model));
            animator->setOwnerNamespace(player->info(), DE_STR("__player__"));
        }
        else
        {
            animator.reset();
            this->identifier.clear();
        }
    }

    void assetBeingDeleted(Asset &)
    {
        de::trash(animator.release());
    }

    static ModelBank &modelBank()
    {
        return ClientApp::render().modelRenderer().bank();
    }
};

PlayerWeaponAnimator::PlayerWeaponAnimator(ClientPlayer *plr)
    : d(new Impl(plr))
{}

void PlayerWeaponAnimator::setAsset(const String &identifier)
{
    d->setupAsset(identifier);
}

String PlayerWeaponAnimator::assetId() const
{
    return d->identifier;
}

void PlayerWeaponAnimator::stateChanged(const state_s *state)
{
    if (d->animator)
    {
        d->animator->triggerByState(Def_GetStateName(state));
    }
}

StateAnimator &PlayerWeaponAnimator::animator()
{
    DE_ASSERT(hasModel());
    return *d->animator;
}

void PlayerWeaponAnimator::setupVisPSprite(vispsprite_t &spr) const
{
    DE_ASSERT(hasModel());

    spr.type = VPSPR_MODEL2;
    spr.data.model2.model = model();
    spr.data.model2.animator = d->animator.get();

    // Use the plain bob values.
    float bob[2] = { *(float *) gx.GetPointer(DD_PSPRITE_BOB_X),
                     *(float *) gx.GetPointer(DD_PSPRITE_BOB_Y) };

    Vec2f angles(
    /* yaw: */   bob[0] * weaponOffsetScale,
    /* pitch: */ (32 - bob[1]) * weaponOffsetScale * weaponOffsetScaleY / 1000.0f);

    const TimeSpan span = 1.0 / Timer_TicksPerSecond();
    d->angleOffset.setValueIfDifferentTarget(angles, span);

    spr.data.model2.yawAngleOffset   = d->angleOffset.x;
    spr.data.model2.pitchAngleOffset = d->angleOffset.y;
}

void PlayerWeaponAnimator::advanceTime(TimeSpan elapsed)
{
    if (clientPaused) return;

    if (d->animator)
    {
        d->animator->advanceTime(elapsed);
    }
}

bool PlayerWeaponAnimator::hasModel() const
{
    return bool(d->animator);
}

const Model *PlayerWeaponAnimator::model() const
{
    if (!hasModel()) return nullptr;
    return &d->animator->model();
}

} // namespace render
