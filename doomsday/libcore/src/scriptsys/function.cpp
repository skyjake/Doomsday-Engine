/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Function"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/NoneValue"
#include "de/Writer"
#include "de/Reader"
#include "de/Log"

#include <QTextStream>
#include <QMap>

namespace de {

typedef QMap<String, Function::NativeEntryPoint> RegisteredEntryPoints;
static RegisteredEntryPoints entryPoints;

DENG2_PIMPL_NOREF(Function)
{
    /// Argument names.
    Function::Arguments arguments;

    /// The function owns the default values stored in the arguments list.
    Function::Defaults defaults;

    /// The statements of this function.
    Compound compound;

    /// Namespace where the function was created. This global namespace is
    /// used always when executing the function, regardless of where the
    /// function is called.
    Record *globals;

    /// Name of the native function (empty, if this is not a native function).
    String nativeName;

    /// The native entry point.
    Function::NativeEntryPoint nativeEntryPoint;

    Instance() : globals(0), nativeEntryPoint(0)
    {}

    Instance(Function::Arguments const &args, Function::Defaults const &defaults)
        : arguments(args), defaults(defaults), globals(0), nativeEntryPoint(0)
    {}
};

Function::Function() : d(new Instance)
{}

Function::Function(Arguments const &args, Defaults const &defaults) 
    : d(new Instance(args, defaults))
{}

Function::Function(String const &nativeName, Arguments const &args, Defaults const &defaults)
    : d(new Instance(args, defaults))
{
    try
    {
        d->nativeName       = nativeName;
        d->nativeEntryPoint = nativeEntryPoint(nativeName);
    }
    catch(Error const &)
    {
        addRef(-1); // Cancelled construction of the instance; no one has a reference.
        throw;
    }
}

Function::~Function()
{
    // Delete the default argument values.
    DENG2_FOR_EACH(Defaults, i, d->defaults)
    {
        delete i.value();
    }
    if(d->globals)
    {
        // Stop observing the namespace.
        d->globals->audienceForDeletion() -= this;
    }
}

String Function::asText() const
{
    String result;
    QTextStream os(&result);
    os << "(Function " << this << " (";
    DENG2_FOR_EACH_CONST(Arguments, i, d->arguments)
    {
        if(i != d->arguments.begin())
        {
            os << ", ";
        }
        os << *i;
        Defaults::const_iterator def = d->defaults.find(*i);
        if(def != d->defaults.end())
        {
            os << "=" << def.value()->asText();
        }
    }
    os << "))";
    return result;
}

Compound &Function::compound()
{
    return d->compound;
}

Compound const &Function::compound() const
{
    return d->compound;
}

Function::Arguments &Function::arguments()
{
    return d->arguments;
}

Function::Arguments const &Function::arguments() const
{
    return d->arguments;
}

Function::Defaults &Function::defaults()
{
    return d->defaults;
}

Function::Defaults const &Function::defaults() const
{
    return d->defaults;
}

void Function::mapArgumentValues(ArrayValue const &args, ArgumentValues &values) const
{
    DENG2_ASSERT(args.size() > 0);

    DictionaryValue const *labeledArgs = dynamic_cast<DictionaryValue const *>(
        args.elements().front());

    DENG2_ASSERT(labeledArgs != NULL);

    // First use all the unlabeled arguments.
    Arguments::const_iterator k = d->arguments.begin();
    for(ArrayValue::Elements::const_iterator i = args.elements().begin() + 1;
        i != args.elements().end(); ++i)
    {
        values.push_back(*i);
        
        if(k != d->arguments.end())
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
    
    if(values.size() < d->arguments.size())
    {
        // Then apply the labeled arguments, falling back to default values.
        Arguments::const_iterator i = d->arguments.begin();
        // Skip past arguments we already have a value for.
        for(duint count = values.size(); count > 0; --count, ++i) {}
        for(; i != d->arguments.end(); ++i)
        {
            try 
            {
                values.push_back(&labeledArgs->element(TextValue(*i)));
            }
            catch(DictionaryValue::KeyError const &)
            {
                // Check the defaults.
                Defaults::const_iterator k = d->defaults.find(*i);
                if(k != d->defaults.end())
                {
                    values.append(k.value());
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
    if(values.size() != d->arguments.size())
    {
        /// @throw WrongArgumentsError  Wrong number of argument specified.
        throw WrongArgumentsError("Function::mapArgumentValues",
                                  "Expected " + QString::number(d->arguments.size()) +
                                  " arguments, but got " + QString::number(values.size()) +
                                  " arguments in function call");
    }    
}

void Function::setGlobals(Record *globals)
{
    LOG_AS("Function::setGlobals");
    DENG2_ASSERT(globals != 0);
    
    if(!d->globals)
    {
        d->globals = globals;
        d->globals->audienceForDeletion() += this;
    }
    /*
    else if(d->globals != globals)
    {
        LOGDEV_SCR_WARNING("Function was offered a different namespace");
        LOGDEV_SCR_VERBOSE("Function %p's namespace is:\n%s\nOffered namespace is:\n%s")
                << this << d->globals->asText() << globals->asText();
    }*/
}

Record *Function::globals() const
{
    return d->globals;
}

bool Function::isNative() const
{
    return d->nativeEntryPoint != NULL;
}

Value *Function::callNative(Context &context, ArgumentValues const &args) const
{
    DENG2_ASSERT(isNative());
    DENG2_ASSERT(args.size() == d->arguments.size()); // all arguments provided

    Value *result = (d->nativeEntryPoint)(context, args);

    if(!result)
    {
        // Must always return something.
        result = new NoneValue;
    }
    return result;
}

void Function::operator >> (Writer &to) const
{
    // Number of arguments.
    to << duint16(d->arguments.size());

    // Argument names.
    DENG2_FOR_EACH_CONST(Arguments, i, d->arguments)
    {
        to << *i;
    }
    
    // Number of default values.
    to << duint16(d->defaults.size());
    
    // Default values.
    DENG2_FOR_EACH_CONST(Defaults, i, d->defaults)
    {
        to << i.key() << *i.value();
    }
    
    // The statements of the function.
    to << d->compound;

    // The possible native entry point.
    to << d->nativeName;
}

void Function::operator << (Reader &from)
{
    duint16 count = 0;
    
    // Argument names.
    from >> count;
    d->arguments.clear();
    while(count--)
    {
        String argName;
        from >> argName;
        d->arguments.push_back(argName);
    }
    
    // Default values.
    from >> count;
    d->defaults.clear();
    while(count--)
    {
        String name;
        from >> name;
        d->defaults[name] = Value::constructFrom(from);
    }
    
    // The statements.
    from >> d->compound;

    from >> d->nativeName;

    // Restore the entry point.
    if(!d->nativeName.isEmpty())
    {
        d->nativeEntryPoint = nativeEntryPoint(d->nativeName);
    }
}

void Function::recordBeingDeleted(Record &DENG2_DEBUG_ONLY(record))
{
    // The namespace of the record is being deleted.
    DENG2_ASSERT(d->globals == &record);

    d->globals = 0;
}

void Function::registerNativeEntryPoint(String const &name, Function::NativeEntryPoint entryPoint)
{
    entryPoints.insert(name, entryPoint);
}

void Function::unregisterNativeEntryPoint(String const &name)
{
    entryPoints.remove(name);
}

Function::NativeEntryPoint Function::nativeEntryPoint(String const &name)
{
    RegisteredEntryPoints::const_iterator found = entryPoints.constFind(name);
    if(found == entryPoints.constEnd())
    {
        throw UnknownEntryPointError("Function::nativeEntryPoint",
                                     QString("Native entry point '%1' is not available").arg(name));
    }
    return found.value();
}

//----------------------------------------------------------------------------

Function *NativeFunctionSpec::make() const
{
    Function::registerNativeEntryPoint(_nativeName, _entryPoint);
    return new Function(_nativeName, _argNames);
}

Binder::Binder(Record *module) : _module(module), _isOwned(false)
{}

Binder::~Binder()
{
    deinit();
}

Binder &Binder::init(Record &module)
{
    _module = &module;
    return *this;
}

Binder &Binder::initNew()
{
    DENG2_ASSERT(!_isOwned);
    _isOwned = true;
    _module = new Record;
    return *this;
}

void Binder::deinit()
{
    if(_isOwned)
    {
        delete _module;
        _module = 0;
        _isOwned = false;
    }
    foreach(String const &name, _boundEntryPoints)
    {
        Function::unregisterNativeEntryPoint(name);
    }
    _boundEntryPoints.clear();
}

Record &Binder::module() const
{
    DENG2_ASSERT(_module != 0);
    return *_module;
}

Binder &Binder::operator << (NativeFunctionSpec const &spec)
{
    if(_module)
    {
        _boundEntryPoints.insert(spec.nativeName());
        *_module << spec;
    }
    return *this;
}

} // namespace de
