/** @file render/shadervar.cpp  Shader variable.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/shadervar.h"

#include <de/dscript.h>

using namespace de;

DE_STATIC_STRING(DEF_WRAP, "wrap");

/**
 * Animatable variable bound to a GL uniform. The value can have 1...4 float
 * components.
 */
void ShaderVar::init(float value)
{
    values.clear();
    values.append(new AnimationValue(Animation(value, Animation::Linear)));
}

ShaderVar::~ShaderVar()
{
    delete uniform;
}

float ShaderVar::currentValue(int index) const
{
    const auto &val = values.at(index);
    float v = val.anim->animation();
    if (val.wrap.isEmpty())
    {
        return v;
    }
    return val.wrap.wrap(v);
}

void ShaderVar::updateUniform()
{
    switch (values.size())
    {
    case 1:
        *uniform = currentValue(0);
        break;

    case 2:
        *uniform = Vec2f(currentValue(0),
                            currentValue(1));
        break;

    case 3:
        *uniform = Vec3f(currentValue(0),
                            currentValue(1),
                            currentValue(2));
        break;

    case 4:
        *uniform = Vec4f(currentValue(0),
                            currentValue(1),
                            currentValue(2),
                            currentValue(3));
        break;
    }
}

static const char *componentNames[] = { "x", "y", "z", "w" };

void ShaderVar::updateValuePointers(Record &names, const String &varName)
{
    if (values.size() == 1)
    {
        values[0].anim = &names[varName].value<AnimationValue>();
    }
    else
    {
        for (int i = 0; i < values.sizei(); ++i)
        {
            values[i].anim = &names[varName.concatenateMember(componentNames[i])]
                    .value<AnimationValue>();
        }
    }
}

//---------------------------------------------------------------------------------------

void ShaderVars::initVariableFromDefinition(const String &variableName,
                                            const Record &valueDef,
                                            Record &bindingNames)
{
    GLUniform::Type uniformType = GLUniform::Float;
    std::unique_ptr<ShaderVar> var(new ShaderVar);

    // Initialize the appropriate type of value animation and uniform,
    // depending on the "value" key in the definition.
    const Value &initialValue = valueDef.get("value");
    if (const auto *array = maybeAs<ArrayValue>(initialValue))
    {
        switch (array->size())
        {
        default:
            throw DefinitionError(
                "StateAnimator::initVariables",
                stringf("%s: Invalid initial value size (%zu) for render.variable",
                        ScriptedInfo::sourceLocation(valueDef).c_str(),
                        array->size()));

        case 2:
            var->init(vectorFromValue<Vec2f>(*array));
            uniformType = GLUniform::Vec2;
            break;

        case 3:
            var->init(vectorFromValue<Vec3f>(*array));
            uniformType = GLUniform::Vec3;
            break;

        case 4:
            var->init(vectorFromValue<Vec4f>(*array));
            uniformType = GLUniform::Vec4;
            break;
        }

        // Expose the components individually in the namespace for scripts.
        for (duint k = 0; k < var->values.size(); ++k)
        {
            addBinding(bindingNames,
                       variableName.concatenateMember(componentNames[k]),
                       var->values[k].anim);
        }
    }
    else
    {
        var->init(float(initialValue.asNumber()));

        // Expose in the namespace for scripts.
        addBinding(bindingNames, variableName, var->values[0].anim);
    }

    // Optional range wrapping.
    if (valueDef.hasSubrecord(DEF_WRAP()))
    {
        for (int k = 0; k < 4; ++k)
        {
            const String varName = DEF_WRAP() + "." + componentNames[k];
            if (valueDef.has(varName))
            {
                var->values[k].wrap = rangeFromValue<Rangef>(valueDef.geta(varName));
            }
        }
    }
    else if (valueDef.has(DEF_WRAP()))
    {
        var->values[0].wrap = rangeFromValue<Rangef>(valueDef.geta(DEF_WRAP()));
    }

    // Uniform to be passed to the shader.
    var->uniform = new GLUniform(variableName.toLatin1(), uniformType);

    // Compose a lookup for quickly finding the variables of each pass
    // (by pass name).
    members[variableName] = std::move(var);
}

void ShaderVars::addBinding(Record &names, const String &varName, AnimationValue *anim)
{
    names.add(varName).set(anim).setReadOnly();
}
