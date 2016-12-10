/** @file animationvalue.h  Value that holds an Animation.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_ANIMATIONVALUE_H
#define LIBDENG2_ANIMATIONVALUE_H

#include "../Value"
#include "../Animation"

namespace de {

/**
 * Value that holds an Animation.
 * @ingroup data
 */
class DENG2_PUBLIC AnimationValue : public Value
{
public:
    AnimationValue(Animation const &animation = Animation());

    /// Returns the time of the value.
    Animation &animation() { return _anim; }

    /// Returns the time of the value.
    Animation const &animation() const { return _anim; }

    Record *memberScope() const override;
    Text typeId() const override;
    Value *duplicate() const override;
    Text asText() const override;
    Number asNumber() const override;
    bool isTrue() const override;
    dint compare(Value const &value) const override;

    // Implements ISerializable.
    void operator >> (Writer &to) const override;
    void operator << (Reader &from) override;

private:
    Animation _anim;
};

} // namespace de

#endif // LIBDENG2_ANIMATIONVALUE_H
