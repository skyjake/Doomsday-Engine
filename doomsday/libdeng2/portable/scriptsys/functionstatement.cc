/*
 * The Doomsday Engine Project -- Hawthorn
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "dmethodstatement.hh"
#include "dmethod.hh"
#include "dcontext.hh"
#include "dprocess.hh"
#include "dconstantexpression.hh"
#include "dtextvalue.hh"
#include "darrayvalue.hh"
#include "ddictionaryvalue.hh"

using namespace de;

MethodStatement::MethodStatement(const std::string& path)
    : path_(path)
{}

MethodStatement::~MethodStatement()
{
    compound_.destroy();
}

void MethodStatement::addArgument(const std::string& argName, Expression* defaultValue)
{
    arguments_.push_back(argName);
    
    if(defaultValue)
    {
        defaults_.add(new ConstantExpression(new TextValue(argName)), defaultValue);
    }
}

void MethodStatement::execute(Context& context) const
{
    Evaluator& eval = context.evaluator();

    // Evaluate the argument default values.
    if(eval.evaluate(&defaults_))
    {
        Method::Defaults defaults;
        DictionaryValue* dict = static_cast<DictionaryValue*>(&eval.result());
        for(DictionaryValue::Elements::const_iterator i = dict->elements().begin(); 
            i != dict->elements().end(); ++i)
        {
            defaults[i->first.value->asText()] = i->second->duplicate();
        }
        
        context.process().newMethod(path_, arguments_, defaults, compound_.firstStatement());
        context.proceed();
    }
}
