/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/function.h"
#include "de/textvalue.h"
#include "de/arrayvalue.h"
#include "de/dictionaryvalue.h"
#include "de/nonevalue.h"
#include "de/scripting/functionvalue.h"
#include "de/writer.h"
#include "de/reader.h"
#include "de/log.h"

#include <sstream>

namespace de {

using RegisteredEntryPoints = KeyMap<String, Function::NativeEntryPoint>;

static RegisteredEntryPoints &entryPoints()
{
    static RegisteredEntryPoints *eps = nullptr;
    if (!eps) eps = new RegisteredEntryPoints;
    return *eps;
}

DE_PIMPL_NOREF(Function)
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
    Record *globals{nullptr};

    /// Name of the native function (empty, if this is not a native function).
    String nativeName;

    /// The native entry point.
    Function::NativeEntryPoint nativeEntryPoint{nullptr};

    Impl() {}

    Impl(const Function::Arguments &args, const Function::Defaults &defaults)
        : arguments(args), defaults(defaults)
    {}
};

Function::Function() : d(new Impl)
{}

Function::Function(const Arguments &args, const Defaults &defaults)
    : d(new Impl(args, defaults))
{}

Function::Function(const String &nativeName, const Arguments &args, const Defaults &defaults)
    : d(new Impl(args, defaults))
{
    try
    {
        d->nativeName       = nativeName;
        d->nativeEntryPoint = nativeEntryPoint(nativeName);
    }
    catch (const Error &)
    {
        addRef(-1); // Cancelled construction of the instance; no one has a reference.
        throw;
    }
}

Function::~Function()
{
    // Delete the default argument values.
    for (auto &i : d->defaults)
    {
        delete i.second;
    }
}

String Function::asText() const
{
    std::ostringstream os;
    os << "(Function " << this << " (";
    for (const auto &i : d->arguments)
    {
        if (&i != &d->arguments.front())
        {
            os << ", ";
        }
        os << i;
        auto def = d->defaults.find(i);
        if (def != d->defaults.end())
        {
            os << "=" << def->second->asText();
        }
    }
    os << "))";
    return os.str();
}

Compound &Function::compound()
{
    return d->compound;
}

const Compound &Function::compound() const
{
    return d->compound;
}

Function::Arguments &Function::arguments()
{
    return d->arguments;
}

const Function::Arguments &Function::arguments() const
{
    return d->arguments;
}

Function::Defaults &Function::defaults()
{
    return d->defaults;
}

const Function::Defaults &Function::defaults() const
{
    return d->defaults;
}

void Function::mapArgumentValues(const ArrayValue &args, ArgumentValues &values) const
{
    DE_ASSERT(args.size() > 0);

    const DictionaryValue *labeledArgs = dynamic_cast<const DictionaryValue *>(
        args.elements().front());

    DE_ASSERT(labeledArgs != nullptr);

    // First use all the unlabeled arguments.
    Arguments::const_iterator k = d->arguments.begin();
    for (ArrayValue::Elements::const_iterator i = args.elements().begin() + 1;
        i != args.elements().end(); ++i)
    {
        values.push_back(*i);

        if (k != d->arguments.end())
        {
            if (labeledArgs->contains(TextValue(*k)))
            {
                /// @throw WrongArgumentsError An argument has been given more than one value.
                throw WrongArgumentsError("Function::mapArgumentValues",
                    "More than one value has been given for '" +
                    *k + "' in function call");
            }
            ++k;
        }
    }

    if (values.size() < d->arguments.size())
    {
        // Then apply the labeled arguments, falling back to default values.
        Arguments::const_iterator i = d->arguments.begin();
        // Skip past arguments we already have a value for.
        for (duint count = values.size(); count > 0; --count, ++i) {}
        for (; i != d->arguments.end(); ++i)
        {
            const TextValue argName(*i);
            if (const Value *labeledValue = labeledArgs->find(argName))
            {
                values.push_back(labeledValue);
            }
            else
            {
                // Check the defaults.
                auto k = d->defaults.find(*i);
                if (k != d->defaults.end())
                {
                    values.append(k->second);
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
    if (values.size() != d->arguments.size())
    {
        /// @throw WrongArgumentsError  Wrong number of argument specified.
        throw WrongArgumentsError("Function::mapArgumentValues",
                                  "Expected " + String::asText(d->arguments.size()) +
                                  " arguments, but got " + String::asText(values.size()) +
                                  " arguments in function call");
    }
}

void Function::setGlobals(Record *globals)
{
    LOG_AS("Function::setGlobals");
    DE_ASSERT(globals);

    if (!d->globals)
    {
        d->globals = globals;
        d->globals->audienceForDeletion() += this;
    }
    /*
    else if (d->globals != globals)
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
    return d->nativeEntryPoint != nullptr;
}

Value *Function::callNative(Context &context, const ArgumentValues &args) const
{
    DE_ASSERT(isNative());
    DE_ASSERT(args.size() == d->arguments.size()); // all arguments provided

    Value *result = (d->nativeEntryPoint)(context, args);

    if (!result)
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
    DE_FOR_EACH_CONST(Arguments, i, d->arguments)
    {
        to << *i;
    }

    // Number of default values.
    to << duint16(d->defaults.size());

    // Default values.
    for (const auto &i : d->defaults)
    {
        to << i.first << *i.second;
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
    while (count--)
    {
        String argName;
        from >> argName;
        d->arguments.push_back(argName);
    }

    // Default values.
    from >> count;
    d->defaults.clear();
    while (count--)
    {
        String name;
        from >> name;
        d->defaults[name] = Value::constructFrom(from);
    }

    // The statements.
    from >> d->compound;

    from >> d->nativeName;

    // Restore the entry point.
    if (!d->nativeName.isEmpty())
    {
        d->nativeEntryPoint = nativeEntryPoint(d->nativeName);
    }
}

void Function::recordBeingDeleted(Record &DE_DEBUG_ONLY(record))
{
    // The namespace of the record is being deleted.
    DE_ASSERT(d->globals == &record);

    d->globals = nullptr;
}

void Function::registerNativeEntryPoint(const String &name, Function::NativeEntryPoint entryPoint)
{
    entryPoints().insert(name, entryPoint);
}

void Function::unregisterNativeEntryPoint(const String &name)
{
    entryPoints().remove(name);
}

Function::NativeEntryPoint Function::nativeEntryPoint(const String &name)
{
    auto found = entryPoints().find(name);
    if (found == entryPoints().end())
    {
        throw UnknownEntryPointError(
            "Function::nativeEntryPoint",
            stringf("Native entry point '%s' is not available", name.c_str()));
    }
    return found->second;
}

//----------------------------------------------------------------------------

Function *NativeFunctionSpec::make() const
{
    Function::registerNativeEntryPoint(_nativeName, _entryPoint);
    return new Function(_nativeName, _argNames, _argDefaults);
}

Binder::Binder(Record *module, FunctionOwnership ownership) 
    : _module(module)
    , _isOwned(false)
    , _funcOwned(ownership)
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
    DE_ASSERT(!_isOwned);
    _isOwned = true;
    _module = new Record;
    return *this;
}

void Binder::deinit()
{
    if (_funcOwned == FunctionsOwned)
    {
        for (auto *var : _boundFunctions)
        {
            delete var;
        }
        _boundFunctions.clear();
    }
    if (_isOwned)
    {
        delete _module;
        _module = nullptr;
        _isOwned = false;
    }
    for (const String &name : _boundEntryPoints)
    {
        Function::unregisterNativeEntryPoint(name);
    }
    _boundEntryPoints.clear();
}

Record &Binder::module() const
{
    DE_ASSERT(_module);
    return *_module;
}

Binder &Binder::operator << (const NativeFunctionSpec &spec)
{
    if (_module)
    {
        _boundEntryPoints.insert(spec.nativeName());
        *_module << spec;

        if (_funcOwned == FunctionsOwned)
        {
            _boundFunctions.insert(&(*_module)[spec.name()]);
        }
    }
    return *this;
}

} // namespace de
