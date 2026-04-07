/** @file animationvalue.h  Value that holds an Animation.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_ANIMATIONVALUE_H
#define LIBCORE_ANIMATIONVALUE_H

#include "de/animation.h"
#include "de/counted.h"
#include "de/value.h"

namespace de {

/**
 * Value that holds an Animation.
 * @ingroup data
 */
class DE_PUBLIC AnimationValue : public Value
{
public:
    AnimationValue(const Animation &animation = Animation());

    ~AnimationValue();

    /// Returns the time of the value.
    Animation &animation() { return *_anim; }

    /// Returns the time of the value.
    const Animation &animation() const { return *_anim; }

    Record *memberScope() const override;
    Text typeId() const override;
    Value *duplicate() const override;
    Value *duplicateAsReference() const override;
    Text asText() const override;
    Number asNumber() const override;
    bool isTrue() const override;
    dint compare(const Value &value) const override;

    // Implements ISerializable.
    void operator >> (Writer &to) const override;
    void operator << (Reader &from) override;

private:
    struct DE_PUBLIC CountedAnimation : public Animation, public Counted {
        CountedAnimation(const Animation &anim);
    };

    AnimationValue(CountedAnimation *countedAnim);

    CountedAnimation *_anim;
};

} // namespace de

#endif // LIBCORE_ANIMATIONVALUE_H
