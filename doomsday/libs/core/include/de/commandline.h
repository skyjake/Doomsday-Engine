/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_COMMANDLINE_H
#define LIBCORE_COMMANDLINE_H

#include <string>
#include <vector>
#include <map>
#include <the_Foundation/process.h>

#include "de/libcore.h"
#include "de/string.h"
#include "de/nativepath.h"

namespace de {

/**
 * Stores and provides access to the command line arguments passed
 * to an application at launch.
 *
 * @ingroup core
 */
class DE_PUBLIC CommandLine
{
public:
    /// Tried to access an argument that does not exist. @ingroup errors
    DE_ERROR(OutOfRangeError);

    /// Execution of the command line failed. @ingroup errors
    DE_ERROR(ExecuteError);

    struct DE_PUBLIC ArgWithParams {
        dint pos; ///< Position of the argument.
        String arg;
        StringList params;

        ArgWithParams();
        operator dint() const;
        dint size() const;
    };

public:
    CommandLine();

    /**
     * Constructs a CommandLine out of a list of strings. The argument
     * strings that begin with a @@ character are parsed as response files
     * the rest are used without modification.
     *
     * @param args  Arguments to use.
     */
    CommandLine(const StringList &args);

    CommandLine(const CommandLine &other);

    /**
     * Returns the native path where the command line was started in.
     * @return Native startup location.
     */
    NativePath startupPath();

    /**
     * Returns the number of arguments. This includes the program name, which
     * is the first argument in the list.
     */
    dsize count() const;

    inline dsize size() const { return count(); }

    dint sizei() const;

    void clear();

    /**
     * Appends a new argument to the list of arguments.
     *
     * @param arg  Argument to append.
     */
    void append(const String &arg);

    CommandLine &operator << (const String &arg) {
        append(arg);
        return *this;
    }

    /**
     * Inserts a new argument to the list of arguments at index @a pos.
     *
     * @param pos  Index at which the new argument will be at.
     * @param arg  Argument to insert.
     */
    void insert(dsize pos, const String &arg);

    /**
     * Removes an argument by index.
     *
     * @param pos  Index of argument to remove.
     */
    void remove(dsize pos);

    /**
     * Checks whether @a arg is in the arguments. Since the first argument is
     * the program name, it is not included in the search.
     *
     * @param arg  Argument to look for. Don't use aliases here.
     * @param count  Number of parameters (non-option arguments) that follow
     *      the located argument.
     *
     * @see isOption()
     *
     * @return  Index of the argument, if found. Otherwise zero.
     */
    ArgWithParams check(const String &arg, dint count = 0) const;

    /**
     * Calls a callback function for each of the parameters given to an argumnet.
     * If there are multiple @a arg options found, all of the parameters given
     * to each option get called in order.
     *
     * @param arg  Argument to look for. Don't use aliases here.
     * @param paramHandler  Callback for the parameters.
     *
     * @return Number of parameters handled.
     */
    int forAllParameters(const String &arg,
                         const std::function<void (dsize, const String &)>& paramHandler) const;

    /**
     * Gets the parameter for an argument.
     *
     * @param arg    Argument to look for. Don't use aliases here. Defines
     *               aliases will be checked for matches.
     * @param param  The parameter is returned here, if found.
     *
     * @return  @c true, if parameter was successfully returned.
     *      Otherwise @c false, in which case @a param is not modified.
     */
    bool getParameter(const String &arg, String &param) const;

    /**
     * Determines whether @a arg exists in the list of arguments.
     *
     * @param arg  Argument to look for. Don't use aliases here.
     *
     * @return  Number of times @a arg is found in the arguments.
     */
    dint has(const String &arg) const;

    /**
     * Determines whether an argument is an option, i.e., it begins with a hyphen.
     */
    bool isOption(dsize pos) const;

    /**
     * Determines whether an argument is an option, i.e., it begins with a hyphen.
     */
    static bool isOption(const String &arg);

    String at(dsize pos) const;

    /**
     * Returns a list of pointers to the arguments. The list contains
     * count() strings and is NULL-terminated.
     */
    const char *const *argv() const;

    /**
     * Converts the argument at position @a pos into an absolute native path.
     * Relative paths are converted relative to the directory that was
     * current at the time the CommandLine was created.
     *
     * @param pos  Argument index.
     */
    void makeAbsolutePath(dsize pos);

    /**
     * Reads a native file and parses its contents using parse().
     *
     * @param nativePath  File to parse.
     */
    void parseResponseFile(const NativePath &nativePath);

    /**
     * Breaks down a single string containing arguments.
     *
     * Examples of behavior:
     * - -cmd "echo ""this is a command""" => [-cmd] [echo "this is a command"]
     * - Hello" My"Friend => [Hello MyFriend]
     * - @@test.rsp [reads contents of test.rsp]
     * - <tt>@@\\"Program Files"\\test.rsp</tt> [reads contents of <tt>"\Program Files\test.rsp"</tt>]
     *
     * @param cmdLine  String containing the arguments.
     */
    void parse(const String &cmdLine);

    /**
     * Defines a new alias for a full argument.
     *
     * @param full  The full argument, e.g., "--help"
     * @param alias  Alias for the full argument, e.g., "-h"
     */
    void alias(const String &full, const String &alias);

    bool isAliasDefinedFor(const String &full) const;

    /**
     * @return @c true, iff the two parameters are equivalent according to
     * the abbreviations.
     */
    bool matches(const String &full, const String &fullOrAlias) const;

    /**
     * Spawns a new process using the command line. The first argument
     * specifies the file name of the executable. Returns immediately
     * after the process has been started.
     *
     * @return @c true if successful, otherwise @c false.
     */
    bool execute() const;

    /**
     * Spawns a new process using the command line and blocks until it
     * finishes running.
     *
     * @param output  Output produced by the started process.
     *
     * @return @c true if successful, otherwise @c false.
     */
    bool executeAndWait(String *output = nullptr) const;

    iProcess *executeProcess() const;

    static CommandLine &get();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_COMMANDLINE_H */
