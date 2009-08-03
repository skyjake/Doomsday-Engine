/*
 * The Doomsday Engine Project -- libdeng2
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

#include "de/Function"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"

#include <sstream>

using namespace de;

Function::Function() : globals_(0)
{}

Function::Function(const Arguments& args, const Defaults& defaults) 
    : arguments_(args), defaults_(defaults), globals_(0)
{}

Function::~Function()
{
    // Delete the default argument values.
    for(Defaults::iterator i = defaults_.begin(); i != defaults_.end(); ++i)
    {
        delete i->second;
    }
    if(globals_)
    {
        // Stop observing the namespace.
        globals_->observers.remove(this);
    }
}

String Function::asText() const
{
    std::ostringstream os;
    os << "[Function " << this << " (";
    for(Arguments::const_iterator i = arguments_.begin(); i != arguments_.end(); ++i)
    {
        if(i != arguments_.begin())
        {
            os << ", ";
        }
        os << *i;
        Defaults::const_iterator def = defaults_.find(*i);
        if(def != defaults_.end())
        {
            os << "=" << def->second->asText();
        }
    }
    os << ")]";
    return os.str();
}

void Function::mapArgumentValues(const ArrayValue& args, ArgumentValues& values)
{
    const DictionaryValue* labeledArgs = dynamic_cast<const DictionaryValue*>(
        args.elements().front());
    assert(labeledArgs != NULL);

    // First use all the unlabeled arguments.
    Arguments::const_iterator k = arguments_.begin();
    for(ArrayValue::Elements::const_iterator i = args.elements().begin() + 1;
        i != args.elements().end(); ++i)
    {
        values.push_back(*i);
        
        if(k != arguments_.end())
        {
            if(labeledArgs->contains(TextValue(*k)))
            {
                /// @throw WrongArgumentsError An argument has been given more than one value.
                throw WrongArgumentsError("Function::mapArgumentValues",
                    "More than one value has been given for '" + 
                    *k + "' in function call");
            }
            ++k;
        }
    }
    
    if(values.size() < arguments_.size())
    {
        // Then apply the labeled arguments, falling back to default values.
        Arguments::const_iterator i = arguments_.begin();
        // Skip past arguments we already have a value for.
        for(duint count = values.size(); count > 0; --count, ++i);
        for(; i != arguments_.end(); ++i)
        {
            try 
            {
                values.push_back(&labeledArgs->element(TextValue(*i)));
            }
            catch(const DictionaryValue::KeyError&)
            {
                // Check the defaults.
                Defaults::const_iterator k = defaults_.find(*i);
                if(k != defaults_.end())
                {
                    values.push_back(k->second);
                }
                else
                {
                    /// @throw WrongArgumentsError Argument is missing a value.
                    throw WrongArgumentsError("Function::mapArgumentValues",
                        "The value of argument '" + *i + "' has not been defined in function call");
                }
            }
        }
    }
    
    // Check that the number of arguments matches what we expect.
    if(values.size() != arguments_.size())
    {
        std::ostringstream os;
        os << "Expected " << arguments_.size() << " arguments, but got " <<
            values.size() << " arguments in function call";
        /// @throw WrongArgumentsError  Wrong number of argument specified.
        throw WrongArgumentsError("Function::mapArgumentValues", os.str());
    }    
}

void Function::setGlobals(Record* globals)
{
    if(!globals_)
    {
        globals_ = globals;
        globals_->observers.add(this);
    }
    else if(globals_ != globals)
    {
        std::cerr << "Function::setGlobals: WARNING! Function was offered a different namespace.\n";
    }
}

Record* Function::globals()
{
    return globals_;
}

bool Function::callNative(Context& context, const ArgumentValues& args)
{
    assert(args.size() == arguments_.size());    
    
    // Do non-native function call.
    return false;
}

void Function::recordBeingDeleted(Record& record)
{
    // The namespace of the record is being deleted.
    assert(globals_ == &record);
    globals_ = 0;
}
