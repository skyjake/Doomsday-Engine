/** @file render/shadervar.cpp  Shader variable.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/ScriptedInfo>
#include <de/ScriptSystem>

using namespace de;

static String const DEF_WRAP("wrap");

/**
 * Animatable variable bound to a GL uniform. The value can have 1...4 float
 * components.
 */
void ShaderVar::init(float value)
{
    values.clear();
    values.append(Animation(value, Animation::Linear));
}

ShaderVar::~ShaderVar()
{
    delete uniform;
}

float ShaderVar::currentValue(int index) const
{
    auto const &val = values.at(index);
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
        *uniform = Vector2f(currentValue(0),
                            currentValue(1));
        break;

    case 3:
        *uniform = Vector3f(currentValue(0),
                            currentValue(1),
                            currentValue(2));
        break;

    case 4:
        *uniform = Vector4f(currentValue(0),
                            currentValue(1),
                            currentValue(2),
                            currentValue(3));
        break;
    }
}

void ShaderVars::initVariableFromDefinition(String const &variableName,
                                            Record const &valueDef,
                                            Record &bindingNames)
{
    static char const *componentNames[] = { "x", "y", "z", "w" };

    GLUniform::Type uniformType = GLUniform::Float;
    std::unique_ptr<ShaderVar> var(new ShaderVar);

    // Initialize the appropriate type of value animation and uniform,
    // depending on the "value" key in the definition.
    Value const &initialValue = valueDef.get("value");
    if (auto const *array = initialValue.maybeAs<ArrayValue>())
    {
        switch (array->size())
        {
        default:
            throw DefinitionError("StateAnimator::initVariables",
                                  QString("%1: Invalid initial value size (%2) for render.variable")
                                  .arg(ScriptedInfo::sourceLocation(valueDef))
                                  .arg(array->size()));

        case 2:
            var->init(vectorFromValue<Vector2f>(*array));
            uniformType = GLUniform::Vec2;
            break;

        case 3:
            var->init(vectorFromValue<Vector3f>(*array));
            uniformType = GLUniform::Vec3;
            break;

        case 4:
            var->init(vectorFromValue<Vector4f>(*array));
            uniformType = GLUniform::Vec4;
            break;
        }

        // Expose the components individually in the namespace for scripts.
        for (int k = 0; k < var->values.size(); ++k)
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
    if (valueDef.hasSubrecord(DEF_WRAP))
    {
        for (int k = 0; k < 4; ++k)
        {
            String const varName = QString("%1.%2").arg(DEF_WRAP).arg(componentNames[k]);
            if (valueDef.has(varName))
            {
                var->values[k].wrap = rangeFromValue<Rangef>(valueDef.geta(varName));
            }
        }
    }
    else if (valueDef.has(DEF_WRAP))
    {
        var->values[0].wrap = rangeFromValue<Rangef>(valueDef.geta(DEF_WRAP));
    }

    // Uniform to be passed to the shader.
    var->uniform = new GLUniform(variableName.toLatin1(), uniformType);

    // Compose a lookup for quickly finding the variables of each pass
    // (by pass name).
    members[variableName] = var.release();
}

ShaderVars::~ShaderVars()
{
    qDeleteAll(members.values());
}

void ShaderVars::addBinding(Record &names, String const &varName, AnimationValue *anim)
{
    names.add(varName).set(anim).setReadOnly();
}
