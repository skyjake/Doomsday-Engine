/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_FUNCTION_H
#define LIBDENG2_FUNCTION_H

#include "../ISerializable"
#include "../Counted"
#include "../String"
#include "../Compound"
#include "../Record"

#include <list>
#include <map>

namespace de
{
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
    class Function : public Counted, public ISerializable, DENG2_OBSERVES(Record, Deletion)
    {
    public:
        /// An incorrect number of arguments is given in a function call. @ingroup errors
        DENG2_ERROR(WrongArgumentsError);

        /// An unknown native entry point was specified. @ingroup errors
        DENG2_ERROR(UnknownEntryPointError);

        typedef std::list<String> Arguments;
        typedef std::map<String, Value *> Defaults;
        typedef std::list<Value const *> ArgumentValues;
        
    public:
        Function();
        
        /**
         * Constructor.
         *
         * @param args      Names of the function arguments.
         * @param defaults  Default values for some or all of the arguments.
         */
        Function(Arguments const &args, Defaults const &defaults);

        /**
         * Construct a function that uses a native entry point.
         *
         * @param nativeName  Name of the entry point.
         * @param args        Names of the function arguments.
         * @param defaults    Default values for some or all of the arguments.
         *
         * @see nativeEntryPoint()
         */
        Function(String const &nativeName, Arguments const &args, Defaults const &defaults);

        ~Function();

        /// Returns a human-readable representation of the function.
        String asText() const;
        
        Compound &compound();

        Compound const &compound() const;

        Arguments &arguments();
        
        Arguments const &arguments() const;

        Defaults &defaults();
        
        Defaults const &defaults() const;
      
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
        void mapArgumentValues(ArrayValue const &args, ArgumentValues &values) const;

        /**
         * Sets the global namespace of the function. This is the namespace 
         * where the function was initially created.
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
         *
         * @return @c false, if the context should proceed with the non-native
         *         function call by creating a new execution context and running
         *         the statements of the function there. @c true, if the 
         *         native call handles everything, including placing the 
         *         return value into the evaluator.
         */
        virtual Value *callNative(Context &context, ArgumentValues const &args) const;

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
        typedef Value *(*NativeEntryPoint)(Context &, ArgumentValues const &);

        /**
         * Registers a native entry point.
         *
         * @param name        Name of the entry point. Will be included in
         *                    serialized data.
         * @param entryPoint  Pointer to the entry point. (Not serialized.)
         */
        static void registerNativeEntryPoint(String const &name, NativeEntryPoint entryPoint);

        /**
         * Unregisters a native entry point. This is required for instance when
         * the entry point is located in a plugin and it is being unloaded.
         * Whoever registered the entry point is responsible for making sure
         * the supplied function pointer remains valid.
         *
         * @param name  Name of a previously registered native entry point.
         */
        static void unregisterNativeEntryPoint(String const &name);

        /**
         * Finds a native entry point. The entry point needs to be either one
         * of the built-in entry points or previously registered with
         * registerNativeEntryPoint().
         *
         * @param name  Name of the entry point.
         *
         * @return Native entry point.
         */
        static NativeEntryPoint nativeEntryPoint(String const &name);
        
    private:
        struct Instance;
        Instance *d;
    };
}

#endif /* LIBDENG2_FUNCTION_H */
