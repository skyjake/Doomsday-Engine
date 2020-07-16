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

#ifndef LIBCORE_FUNCTION_H
#define LIBCORE_FUNCTION_H

#include "compound.h"
#include "de/iserializable.h"
#include "de/counted.h"
#include "de/string.h"
#include "de/record.h"
#include "de/list.h"
#include "de/keymap.h"
#include "de/set.h"

namespace de {

class Statement;
class Context;
class Expression;
class Value;
class ArrayValue;

/**
 * Callable set of statements ready for execution, or a wrapper for a native
 * function.
 *
 * A function can either consist of a compound of statements or it can use
 * a native entry point that will perform processing using native code. Both
 * are defined in scripts using the @c def keyword, which allows the script
 * engine to set up all the arguments in the way that the function expects.
 *
 * Functions are reference-counted so that they exist as long as other
 * objects need them (FunctionStatement, FunctionValue).The argument list
 * defines what kind of arguments can be passed to the function and what
 * are the default values for the arguments.
 *
 * @ingroup script
 */
class DE_PUBLIC Function : public Counted, public ISerializable, DE_OBSERVES(Record, Deletion)
{
public:
    /// An incorrect number of arguments is given in a function call. @ingroup errors
    DE_ERROR(WrongArgumentsError);

    /// An unknown native entry point was specified. @ingroup errors
    DE_ERROR(UnknownEntryPointError);

    typedef List<String>         Arguments;
    typedef KeyMap<String, Value *> Defaults;
    typedef List<const Value *>  ArgumentValues;

public:
    Function();

    /**
     * Constructor.
     *
     * @param args      Names of the function arguments.
     * @param defaults  Default values for some or all of the arguments. Ownership
     *                  of the values is transferred to Function.
     */
    Function(const Arguments &args, const Defaults &defaults);

    /**
     * Construct a function that uses a native entry point.
     *
     * @param nativeName  Name of the entry point.
     * @param args        Names of the function arguments.
     * @param defaults    Default values for some or all of the arguments.
     *
     * @see nativeEntryPoint()
     */
    Function(const String &nativeName,
             const Arguments &args = Arguments(),
             const Defaults &defaults = Defaults());

    /// Returns a human-readable representation of the function.
    String asText() const;

    Compound &compound();

    const Compound &compound() const;

    Arguments &arguments();

    const Arguments &arguments() const;

    Defaults &defaults();

    const Defaults &defaults() const;

    /**
     * Maps a set of named and unnamed argument values to the list of values that
     * will be passed to the function. Default values will be used for any arguments
     * that have been given no value. No copies of any values are made.
     *
     * @param args    The array's first element must be a DictionaryValue containing
     *                values for the named arguments of the call. The rest of the array
     *                are the unnamed arguments.
     * @param values  The resulting list of values to the passed to the function.
     *                The values are in the order the arguments have been declared in
     *                the function statement.
     */
    void mapArgumentValues(const ArrayValue &args, ArgumentValues &values) const;

    /**
     * Sets the global namespace of the function. This is the namespace where the
     * function was initially created. Once set, it cannot be changed.
     */
    void setGlobals(Record *globals);

    /**
     * Returns the global namespace of the function.
     * Return @c NULL when the originating namespace has been deleted.
     */
    Record *globals() const;

    /**
     * Determines if this is a native function. callNative() is called to
     * execute native functions instead of pushing a new context on the
     * process's stack.
     *
     * @return @c true, iff this is a native function.
     */
    bool isNative() const;

    /**
     * Name of the native entry point.
     *
     * @return Entry point name (if native).
     */
    String nativeName() const;

    /**
     * Perform a native call of the function.
     *
     * @param context  Execution context. Any results generated by a
     *                 native function are placed here.
     * @param args     Arguments to the function. The array's first element
     *                 is always a dictionary that contains the labeled values.
     *
     * @return Return value from the native function. Always a valid Value.
     */
    virtual Value *callNative(Context &context, const ArgumentValues &args) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    // Observes Record deletion.
    void recordBeingDeleted(Record &record);

public:
    /**
     * Signature for native entry points. If the native function does not
     * produce a return value (returns @c NULL), a NoneValue is
     * automatically created.
     */
    typedef Value *(*NativeEntryPoint)(Context &, const ArgumentValues &);

    /**
     * Registers a native entry point.
     *
     * @param name        Name of the entry point. Will be included in
     *                    serialized data.
     * @param entryPoint  Pointer to the entry point. (Not serialized.)
     */
    static void registerNativeEntryPoint(const String &name, NativeEntryPoint entryPoint);

    /**
     * Unregisters a native entry point. This is required for instance when
     * the entry point is located in a plugin and it is being unloaded.
     * Whoever registered the entry point is responsible for making sure
     * the supplied function pointer remains valid.
     *
     * @param name  Name of a previously registered native entry point.
     */
    static void unregisterNativeEntryPoint(const String &name);

    /**
     * Finds a native entry point. The entry point needs to be either one
     * of the built-in entry points or previously registered with
     * registerNativeEntryPoint().
     *
     * @param name  Name of the entry point.
     *
     * @return Native entry point.
     */
    static NativeEntryPoint nativeEntryPoint(const String &name);

protected:
    ~Function(); // Counted

private:
    DE_PRIVATE(d)
};

/**
 * Utility for storing information about a native function entry point and its
 * correspondig script function equivalent.
 *
 * @ingroup script
 */
class DE_PUBLIC NativeFunctionSpec
{
public:
    NativeFunctionSpec(Function::NativeEntryPoint entryPoint,
                       const char *nativeName,
                       const String &name,
                       const Function::Arguments &argNames = Function::Arguments(),
                       const Function::Defaults &argDefaults = Function::Defaults())
        : _entryPoint(entryPoint)
        , _nativeName(nativeName)
        , _name(name)
        , _argNames(argNames)
        , _argDefaults(argDefaults)
    {}

    /**
     * Makes a new native Function according to the specification.
     * @return Caller gets ownership (ref 1).
     */
    Function *make() const;

    const char *nativeName() const { return _nativeName; }
    String name() const { return _name; }

private:
    Function::NativeEntryPoint _entryPoint;
    const char *_nativeName;
    String _name;
    Function::Arguments _argNames;
    Function::Defaults _argDefaults;
};

#define DE_FUNC_NOARG(Name, ScriptMemberName) \
    de::NativeFunctionSpec(Function_ ## Name, # Name, ScriptMemberName)

#define DE_FUNC(Name, ScriptMemberName, Args) \
    de::NativeFunctionSpec(Function_ ## Name, # Name, ScriptMemberName, de::Function::Arguments() << Args)

#define DE_FUNC_DEFS(Name, ScriptMemberName, Args, Defaults) \
    de::NativeFunctionSpec(Function_ ## Name, # Name, ScriptMemberName, \
                           de::Function::Arguments() << Args, Defaults)

/**
 * Utility that keeps track of which entry points have been bound and
 * unregisters them when the Binder instance is destroyed. For example, use as
 * a member in a class that registers native entry points.
 *
 * @ingroup script
 */
class DE_PUBLIC Binder
{
public:
    enum FunctionOwnership { FunctionsOwned, FunctionsNotOwned };

    /**
     * @param module  Module to associate with the Binder at construction.
     *                The module is not owned by the Binder.
     * @param ownsFunctions  Binder has ownership of created Functions and
     *                and will delete them when the Binder object is
     *                deleted.
     */
    Binder(Record *module = nullptr, FunctionOwnership ownership = FunctionsNotOwned);

    /**
     * Automatically deinitializes the Binder before destroying.
     */
    ~Binder();

    /**
     * Initializes the Binder for making new native function bindings to a module.
     * The module will not be owned by the Binder.
     *
     * @param module  Module to bind to.
     *
     * @return Reference to this instance.
     */
    Binder &init(Record &module);

    /**
     * Initializes the Binder with a completely new module. The new module is owned
     * by the Binder and will be deleted when the Binder instance is destroyed.
     *
     * @return Reference to this instance.
     */
    Binder &initNew();

    /**
     * Deinitialiazes the bindings. All native entry points registered using the
     * Binder are automatically unregistered.
     */
    void deinit();

    Record &module() const;

    Binder &operator << (const NativeFunctionSpec &spec);

private:
    Record *_module;
    bool _isOwned;
    FunctionOwnership _funcOwned;
    Set<String> _boundEntryPoints;
    Set<Variable *> _boundFunctions;
};

} // namespace de

#endif /* LIBCORE_FUNCTION_H */
