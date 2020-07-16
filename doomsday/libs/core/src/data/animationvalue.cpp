/** @file animationvalue.cpp  Value that holds an Animation.
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

#include "de/animationvalue.h"
#include "de/reader.h"
#include "de/scripting/scriptsystem.h"
#include "de/writer.h"

namespace de {

AnimationValue::AnimationValue(const Animation &anim)
    : _anim(new CountedAnimation(anim))
{}

AnimationValue::~AnimationValue()
{
    releaseRef(_anim);
}

AnimationValue::AnimationValue(CountedAnimation *countedAnim)
    : _anim(holdRef(countedAnim))
{}

Record *AnimationValue::memberScope() const
{
    return &ScriptSystem::builtInClass("Animation");
}

Value *AnimationValue::duplicate() const
{
    return new AnimationValue(*static_cast<Animation *>(_anim)); // makes a separate Animation
}

Value *AnimationValue::duplicateAsReference() const
{
    return new AnimationValue(_anim); // refers to the same Animation
}

Value::Text AnimationValue::asText() const
{
    return _anim->asText();
}

Value::Number AnimationValue::asNumber() const
{
    return Number(_anim->value());
}

bool AnimationValue::isTrue() const
{
    return !_anim->done();
}

dint AnimationValue::compare(const Value &value) const
{
    // Compare using the current numerical value.
    const Number a = asNumber();
    const Number b = value.asNumber();
    if (fequal(a, b)) return 0;
    return (a < b? -1 : +1);
}

void AnimationValue::operator >> (Writer &to) const
{
    to << SerialId(ANIMATION) << *_anim;
}

void AnimationValue::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if (id != ANIMATION)
    {
        throw DeserializationError("AnimationValue::operator <<", "Invalid ID");
    }
    from >> *_anim;
}

Value::Text AnimationValue::typeId() const
{
    return "Animation";
}

//---------------------------------------------------------------------------------------

AnimationValue::CountedAnimation::CountedAnimation(const Animation &anim)
    : Animation(anim)
{}

} // namespace de
