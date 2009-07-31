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

#include "dnameexpression.hh"
#include "dobject.hh"
#include "dmethod.hh"
#include "devaluator.hh"
#include "dprocess.hh"
#include "dmethodvalue.hh"
#include "dobjectvalue.hh"
#include "dvariablevalue.hh"
#include "dobject.hh"
#include "dnodeobject.hh"
#include "dvariable.hh"
#include "dkernel.hh"
#include "dlog.hh"

using namespace de;

NameExpression::NameExpression(const std::string& path, int flags) 
    : path_(path), flags_(flags)
{}

Value* NameExpression::evaluate(Evaluator& evaluator) const
{
    LOG_AS("NameExpression::evaluator");
    LOG_DEBUG("path = %s, scope = %x") << path_ << evaluator.names();
    
    Process& proc = evaluator.process();
    Process::FolderNodePair ptr(NULL, NULL);
    
    Folder* localScope = (evaluator.names()? evaluator.names() : &evaluator.context().names());
    
    if(flags_ & LOCAL_ONLY)
    {
        // Just look in the local namespace.
        ptr = proc.findNodeInNames(path_, localScope);
    }
    else
    {
        // Look everywhere (except when the scope is set).
        // The Folder of the pair is the value of "self".
        ptr = proc.findNodeInNames(path_, evaluator.names());
    }

    // If nothing is found and we are permitted to create new variables, do so.
    // Used for assignment into new variables.
    if(!ptr.first && !ptr.second && (flags_ & NEW_VARIABLE))
    {
        Variable* var = proc.newVariable(*localScope, path_, new ObjectValue());
        ptr = Process::FolderNodePair(
            Process::getContainingFolder(*localScope, path_), var);
    }

    Variable* var = dynamic_cast<Variable*>(ptr.second);
    if(var)
    {
        if(flags_ & BY_REFERENCE)
        {
            // Reference to the variable itself.
            return new VariableValue(*var);
        }
        else
        {
            // Variables evaluate to their values.
            return var->value().duplicate();
        }
    }
    
    Method* method = dynamic_cast<Method*>(ptr.second);
    if(method)
    {
        assert(ptr.first != NULL);

        // The self object must be an Object.
        // At this stage, the folder is probably a variable, so we need
        // to use the object that represents the contents of the folder.
        // The same logic is used when looking for nodes.
        Object* self = dynamic_cast<Object*>(ptr.first);
        assert(self != NULL);

        return new MethodValue(*method, *self);
    }
    
    // Check for a special method: object instantion.
    Object* cls = dynamic_cast<Object*>(ptr.second);
    if(cls && cls->instantiable())
    {
        return new ObjectValue(cls);           
    }        
    
    // It's a node, still?
    if(ptr.second)
    {
        Object* object =
            proc.kernel().names().node<NodeObject>("Node")->instantiate(
                ptr.second->path(), evaluator.context());
        Value* value = evaluator.popResult();
        // The value now has a reference, we don't need one.
        object->release();
        return value;
    }

    throw NotFoundError("NameExpression::evaluate", "Node '" + path_ + "' does not exist");
}
