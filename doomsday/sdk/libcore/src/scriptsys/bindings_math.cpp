/** @file bindings_math.cpp  Built-in Math module.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../src/scriptsys/bindings_math.h"
#include "de/math.h"

#include <de/NumberValue>

namespace de {

static Value *Function_Math_Random(Context &, Function::ArgumentValues const &)
{
    return new NumberValue(randf());
}

static Value *Function_Math_RandInt(Context &, Function::ArgumentValues const &args)
{
    Rangei range(args.at(0)->asInt(), args.at(1)->asInt() + 1);
    return new NumberValue(range.random());
}

static Value *Function_Math_RandNum(Context &, const Function::ArgumentValues &args)
{
    Ranged range(args.at(0)->asNumber(), args.at(1)->asNumber());
    return new NumberValue(range.random());
}

static Value *Function_Math_Cos(Context &, const Function::ArgumentValues &args)
{
    return new NumberValue(std::cos(args.at(0)->asNumber()));
}

static Value *Function_Math_Sin(Context &, const Function::ArgumentValues &args)
{
    return new NumberValue(std::sin(args.at(0)->asNumber()));
}

static Value *Function_Math_Tan(Context &, const Function::ArgumentValues &args)
{
    return new NumberValue(std::tan(args.at(0)->asNumber()));
}

void initMathModule(Binder &binder, Record &mathModule)
{
    binder.init(mathModule)
            << DENG2_FUNC_NOARG(Math_Random, "random")
            << DENG2_FUNC      (Math_RandInt, "randInt", "low" << "high")
            << DENG2_FUNC      (Math_RandNum, "randNum", "low" << "high")
            << DENG2_FUNC      (Math_Cos, "cos", "radians")
            << DENG2_FUNC      (Math_Sin, "sin", "radians")
            << DENG2_FUNC      (Math_Tan, "tan", "radians");
}

} // namespace de
