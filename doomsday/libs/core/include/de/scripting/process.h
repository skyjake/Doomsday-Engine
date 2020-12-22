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

#ifndef LIBCORE_PROCESS_H
#define LIBCORE_PROCESS_H

#include "script.h"
#include "context.h"
#include "evaluator.h"
#include "function.h"
#include "iobject.h"
#include "scriptlex.h"
#include "../time.h"
#include "../string.h"
#include "../variable.h"
#include "../recordvalue.h"

#include <list>

namespace de {

class ArrayValue;

namespace internal {

/**
 * Utility for composing arguments for a script call from native code. This is
 * used by Process:scriptCall() and is not intended to be used manually (hence
 * it is in the de::internal namespace).
 */
struct DE_PUBLIC ScriptArgumentComposer
{
    StringList args;
    int counter = 0;
    Record &ns;

    ScriptArgumentComposer(Record &names) : ns(names) {}

    ~ScriptArgumentComposer()
    {
        // Delete the argument variables that were created.
        for (int i = 0; i < counter; ++i)
        {
            delete ns.remove(stringf("__arg%i__", i));
        }
    }

    Variable &addArgument()
    {
        return ns.add(stringf("__arg%i__", counter++));
    }

    template <typename Type>
    String scriptArgumentAsText(const Type &arg)
    {
        return String::asText(arg); // basic types
    }

    inline void convertScriptArguments(StringList &) {}

    template <typename FirstArg, typename... Args>
    void convertScriptArguments(StringList &list, const FirstArg &firstArg, Args... args)
    {
        list << scriptArgumentAsText(firstArg);
        convertScriptArguments(list, args...);
    }
};

template <>
inline String ScriptArgumentComposer::scriptArgumentAsText(const String &arg)
{
    if (arg.beginsWith("$")) // Verbatim?
    {
        return arg.substr(BytePos(1));
    }
    String quoted(arg);
    quoted.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n");
    return Stringf("\"%s\"", quoted.c_str());
}

template <>
inline String ScriptArgumentComposer::scriptArgumentAsText(const std::nullptr_t &)
{
    return ScriptLex::NONE;
}

template <>
inline String ScriptArgumentComposer::scriptArgumentAsText(const char * const &utf8)
{
    if (!utf8) return ScriptLex::NONE;
    return scriptArgumentAsText(String::fromUtf8(utf8));
}

template <>
inline String ScriptArgumentComposer::scriptArgumentAsText(const Record &record)
{
    Variable &arg = addArgument();
    arg.set(new RecordValue(record));
    return arg.name();
}

template <>
inline String ScriptArgumentComposer::scriptArgumentAsText(const Record * const &record)
{
    if (!record) return ScriptLex::NONE;
    return scriptArgumentAsText(*record);
}

template <>
inline String ScriptArgumentComposer::scriptArgumentAsText(const IObject * const &object)
{
    if (!object) return ScriptLex::NONE;
    return scriptArgumentAsText(object->objectNamespace());
}

template <>
inline String ScriptArgumentComposer::scriptArgumentAsText(const IObject &object)
{
    return scriptArgumentAsText(object.objectNamespace());
}

#define DE_SCRIPT_ARGUMENT_TYPE(ArgType, Method) \
    namespace de { namespace internal { template <> inline String ScriptArgumentComposer::scriptArgumentAsText(ArgType const &arg) { Method } } }

} // namespace internal

/**
 * Executes a script. The process maintains the execution environment, including things
 * like local variables and keeping track of which statement is being executed.
 *
 * @ingroup script
 */
class DE_PUBLIC Process
{
public:
    /// The process is running while an operation is attempted that requires the
    /// process to be stopped. @ingroup errors
    DE_ERROR(NotStoppedError);

    /// Suspending or resuming fails. @ingroup errors
    DE_ERROR(SuspendError);

    /// Execution is taking too long to complete. @ingroup errors
    DE_ERROR(HangError);

    /// A process is always in one of these states.
    enum State {
        Running,    /**< The process is running normally. */
        Suspended,  /**< The process has been suspended and will
                     *   not continue running until restored.  A
                     *   process cannot restore itself from a
                     *   suspended state. */
        Stopped     /**< The process has reached the end of the
                     *   script or has been terminated. */
    };

    using Namespaces = Evaluator::Namespaces;

public:
    /**
     * Constructs a new process. The process is initialized to STOPPED state.
     *
     * @param externalGlobalNamespace  Namespace to use as the global namespace
     *                                 of the process. If not specified, a private
     *                                 global namespace is created for the process.
     *                                 The process does not get ownership of the
     *                                 external global namespace.
     */
    Process(Record *externalGlobalNamespace = nullptr);

    /**
     * Constructs a new process. The process is initialized to RUNNING state.
     *
     * @param script  Script to run. No reference to the script is retained
     *                apart from pointing to its statements. The script instance
     *                must remain in existence while the process is running,
     *                as it is the owner of the statements.
     */
    Process(const Script &script);

    State state() const;

    /// Determines the current depth of the call stack.
    dsize depth() const;

    /**
     * Resets the process to an empty state. All existing content in the
     * process's context stack is removed, leaving the process in a similar
     * state than after construction.
     */
    void clear();

    /**
     * Starts running the given script. Note that the process must be
     * in the FINISHED state for this to be a valid operation.
     *
     * @param script  Script to run. No reference to the script is retained
      *               apart from pointing to its statements. The script instance
      *               must remain in existence while the process is running,
      *               as it is the owner of the statements.
     */
    void run(const Script &script);

    /**
     * Suspends or resumes execution of the script.
     */
    void suspend(bool suspended = true);

    /**
     * Stops the execution of the script. The process is set to the
     * FINISHED state, which means it must be rerun with a new script.
     */
    void stop();

    /**
     * Execute the next command(s) in the script. Execution continues until
     * the script leaves RUNNING state.
     */
    void execute();

    /**
     * Finish the execution of the topmost context. Execution will
     * continue from the second topmost context.
     *
     * @param returnValue  Value to use as the return value from the
     *                     context. Takes ownership of the value.
     */
    void finish(Value *returnValue = nullptr);

    /**
     * Changes the working path of the process. File system access within the
     * process is relative to the working path.
     *
     * @param newWorkingPath  New working path for the process.
     */
    void setWorkingPath(const String &newWorkingPath);

    /**
     * Returns the current working path.
     */
    const String &workingPath() const;

    /**
     * Return an execution context. By default returns the topmost context.
     *
     * @param downDepth  How many levels to go down. There are depth() levels
     *                   in the context stack.
     *
     * @return  Context at @a downDepth.
     */
    Context &context(duint downDepth = 0);

    /**
     * Pushes a new context to the process's stack.
     *
     * @param context  Context. Ownership taken.
     */
    void pushContext(Context *context);

    /**
     * Pops the topmost context off the stack and returns it. Ownership given
     * to caller.
     */
    Context *popContext();

    /**
     * Performs a function call. A new context is created on the context
     * stack and the function's statements are executed on the new stack.
     * After the call is finished, the result value is pushed to the
     * calling context's evaluator.
     *
     * @param function   Function to call.
     * @param arguments  Arguments for the function. The array's first element
     *                   must be a DictionaryValue containing values for the
     *                   named arguments of the call. The rest of the array
     *                   are the unnamed arguments.
     * @param self  Optional scope that becomes the value of the "self"
     *                   variable. Ownership given to Process.
     */
    void call(const Function &function, const ArrayValue &arguments,
              Value *self = 0);

    /**
     * Collects the namespaces currently visible. This includes the process's
     * own stack and the global namespaces.
     *
     * @param spaces  The namespaces are collected here. The order is important:
     *                earlier namespaces shadow the subsequent ones.
     */
    void namespaces(Namespaces &spaces) const;

    /**
     * Returns the global namespace of the process. This is always the bottommost context
     * in the stack.
     */
    Record &globals();

    /**
     * Returns the local namespace of the process. This is always the topmost context in
     * the stack.
     */
    Record &locals();

public:
    /*
     * Utilities for calling script functions from native code.
     */
    enum CallResult { IgnoreResult, TakeResult };

    /**
     * Calls a script function. Native arguments are converted to script
     * source text and then parsed into Values when the call is executed.
     *
     * Only non-named function arguments are supported by this method.
     *
     * The namespace @a global may be modified if some of the argument values
     * require variables, e.g., for refering Records. The created variables
     * are named `__argN__`, with `N` being an increasing number.
     *
     * @param result    What to do with the result value.
     * @param globals   Global namespace where to execute the call.
     * @param function  Name of the function.
     * @param args      Argument values for the function call.
     *
     * @return Depending on @a result, returns nullptr or the return value
     * from the call. Caller gets ownership of the possibly returned Value.
     */
    template <typename... Args>
    static Value *scriptCall(CallResult result, Record &globals,
                             const String &function, Args... args)
    {
        internal::ScriptArgumentComposer composer(globals);
        composer.convertScriptArguments(composer.args, args...);
        Script script(function + "(" + String::join(composer.args, ",") + ")");
        Process proc(&globals);
        proc.run(script);
        proc.execute();
        if (result == IgnoreResult) return nullptr;
        // Return the result using the request value type.
        return proc.context().evaluator().popResult();
    }

    template <typename ReturnValueType, typename... Args>
    static ReturnValueType *scriptCall(Record &globals, const String &function, Args... args)
    {
        return static_cast<ReturnValueType *>(scriptCall(TakeResult, globals, function, args...));
    }

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_PROCESS_H */
