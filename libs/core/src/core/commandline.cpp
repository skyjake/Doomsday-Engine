/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/commandline.h"
#include "de/string.h"
#include "de/nativepath.h"
#include "de/log.h"
#include "de/app.h"

#include <the_Foundation/fileinfo.h>
#include <the_Foundation/stringlist.h>
#include <the_Foundation/path.h> // cygwin paths
#include <the_Foundation/process.h>

#include <fstream>
#include <sstream>
#include <cctype>
#include <string.h>

namespace de {

static char *duplicateStringAsUtf8(const String &s)
{
    return iDupStr(s);
}

DE_PIMPL(CommandLine)
{
    NativePath initialDir;

    typedef StringList Arguments;
    Arguments arguments;

    typedef std::vector<char *> ArgumentPointers; // UTF-8 representation
    ArgumentPointers pointers;

    typedef std::vector<String> ArgumentStrings;
    typedef std::map<std::string, ArgumentStrings> Aliases;
    Aliases aliases;

    Impl(Public &i) : Base(i)
    {
        initialDir = NativePath::workPath();
    }

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        arguments.clear();
        DE_FOR_EACH(ArgumentPointers, i, pointers) free(*i);
        pointers.clear();
        pointers.push_back(nullptr);
    }

    void appendArg(const String &arg)
    {
        arguments.append(arg);

        if (pointers.empty())
        {
            pointers.push_back(duplicateStringAsUtf8(arg));
            pointers.push_back(nullptr); // Keep null-terminated.
        }
        else
        {
            // Insert before the NULL.
            pointers.insert(pointers.end() - 1, duplicateStringAsUtf8(arg));
        }
        DE_ASSERT(pointers.back() == nullptr);
    }

    void insert(dsize pos, const String &arg)
    {
        if (pos > arguments.size())
        {
            /// @throw OutOfRangeError @a pos is out of range.
            throw OutOfRangeError("CommandLine::insert", "Index out of range");
        }

        arguments.insert(pos, arg);

        pointers.insert(pointers.begin() + pos, duplicateStringAsUtf8(arg));
        DE_ASSERT(pointers.back() == nullptr);
    }

    void remove(dsize pos)
    {
        if (pos >= arguments.size())
        {
            /// @throw OutOfRangeError @a pos is out of range.
            throw OutOfRangeError("CommandLine::remove", "Index out of range");
        }

        arguments.removeAt(pos);

        free(pointers[pos]);
        pointers.erase(pointers.begin() + pos);
        DE_ASSERT(pointers.back() == nullptr);
    }

    iProcess *execute() const
    {
        if (self().count() < 1) return nullptr;

        iProcess *proc = new_Process();
        setWorkingDirectory_Process(proc, initialDir.toString());

        auto args = tF::make_ref(new_StringList());
        for (dsize i = 0; i < self().count(); ++i)
        {
            pushBack_StringList(args, self().at(i));
        }
        setArguments_Process(proc, args);

        if (!start_Process(proc))
        {
            iRelease(proc);
            return nullptr;
        }
        return proc;
    }
};

CommandLine::CommandLine() : d(new Impl(*this))
{}

CommandLine::CommandLine(const StringList &args) : d(new Impl(*this))
{
    for (dsize i = 0; i < args.size(); ++i)
    {
        mb_iterator arg = args.at(i);
        if (*arg == '@')
        {
            // This is a response file or something else that requires parsing.
            parseResponseFile(String(++arg));
        }
        else
        {
            d->appendArg(args.at(i));
        }
    }
#if defined (DE_CYGWIN) || defined (DE_MSYS)
    makeAbsolutePath(0); // convert to a Windows path
#endif
}

CommandLine::CommandLine(const CommandLine &other) : d(new Impl(*this))
{
    for (const auto &i : other.d->arguments)
    {
        d->appendArg(i);
    }
}

NativePath CommandLine::startupPath()
{
    return d->initialDir;
}

dsize CommandLine::count() const
{
    return d->arguments.size();
}

dint CommandLine::sizei() const
{
    return dint(count());
}

void CommandLine::clear()
{
    d->clear();
}

void CommandLine::append(const String &arg)
{
    d->appendArg(arg);
}

void CommandLine::insert(dsize pos, const String &arg)
{
    d->insert(pos, arg);
}

void CommandLine::remove(dsize pos)
{
    d->remove(pos);
}

CommandLine::ArgWithParams CommandLine::check(const String &arg, dint numParams) const
{
    // Do a search for arg.
    Impl::Arguments::const_iterator i = d->arguments.begin();
    for (; i != d->arguments.end() && !matches(arg, *i); ++i) {}

    if (i == d->arguments.end())
    {
        // Not found.
        return {};
    }

    // It was found, check for the number of non-option parameters.
    ArgWithParams found;
    found.arg = arg;
    Impl::Arguments::const_iterator k = i;
    while (numParams-- > 0)
    {
        if (++k == d->arguments.end() || isOption(*k))
        {
            // Ran out of arguments, or encountered an option.
            return ArgWithParams();
        }
        found.params.append(*k);
    }

    found.pos = dint(i - d->arguments.begin());
    return found;
}

int CommandLine::forAllParameters(const String &arg,
                                  const std::function<void (dsize, const String &)>& paramHandler) const
{
    int total = 0;
    bool inside = false;

    for (Impl::Arguments::const_iterator i = d->arguments.begin();
         i != d->arguments.end(); ++i)
    {
        if (matches(arg, *i))
        {
            inside = true;
        }
        else if (inside)
        {
            if (isOption(*i))
            {
                inside = false;
            }
            else
            {
                paramHandler(i - d->arguments.begin(), *i);
                ++total;
            }
        }
    }
    return total;
}

bool CommandLine::getParameter(const String &arg, String &param) const
{
    dint pos = check(arg, 1);
    if (pos > 0)
    {
        param = at(pos + 1);
        return true;
    }
    return false;
}

dint CommandLine::has(const String &arg) const
{
    dint howMany = 0;

    DE_FOR_EACH_CONST(Impl::Arguments, i, d->arguments)
    {
        if (matches(arg, *i))
        {
            howMany++;
        }
    }
    return howMany;
}

bool CommandLine::isOption(dsize pos) const
{
    if (pos >= d->arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::isOption", "Index out of range");
    }
    DE_ASSERT(!d->arguments[pos].isEmpty());
    return isOption(d->arguments[pos]);
}

bool CommandLine::isOption(const String &arg)
{
    return !(arg.empty() || arg.first() != '-');
}

String CommandLine::at(dsize pos) const
{
    return d->arguments.at(pos);
}

const char *const *CommandLine::argv() const
{
    DE_ASSERT(*d->pointers.rbegin() == nullptr); // the list itself must be null-terminated
    return &d->pointers[0];
}

void CommandLine::makeAbsolutePath(dsize pos)
{
    if (pos >= d->arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::makeAbsolutePath", "Index out of range");
    }

    String arg = d->arguments[pos];

    if (!isOption(pos) && !arg.beginsWith("}"))
    {
        bool converted = false;

#if defined (DE_CYGWIN) || defined (DE_MSYS)
        // Cygwin gives us UNIX-like paths on the command line, so let's convert
        // to our expected Windows paths.
        arg = String::take(unixToWindows_Path(arg));
#endif

        NativePath dir = NativePath(arg).expand(); // note: strips trailing slash

        if (!dir.isAbsolute())
        {
            dir = d->initialDir / dir;
            converted = true;
        }

        // Update the argument string.
        d->arguments[pos] = dir;

        iFileInfo *info = newCStr_FileInfo(dir);
        if (isDirectory_FileInfo(info))
        {
            // Append a slash so FS1 will treat it as a directory.
            d->arguments[pos] += '/';
        }
        iRelease(info);

        // Replace the pointer string.
        free(d->pointers[pos]);
        d->pointers[pos] = duplicateStringAsUtf8(d->arguments[pos]);

        if (converted)
        {
            LOG_DEBUG("Argument %i converted to absolute path: \"%s\"") << pos << d->pointers[pos];
        }
    }
}

void CommandLine::parseResponseFile(const NativePath &nativePath)
{
    if (std::ifstream response{nativePath.expand()})
    {
        parse(String::fromUtf8(Block::readAll(response)));
    }
    else
    {
        warning("Failed to open response file: %s", nativePath.c_str());
    }
}

void CommandLine::parse(const String &cmdLine)
{
    String::const_iterator i = cmdLine.begin();

    // This is unset if we encounter a terminator token.
    bool isDone = false;

    // Are we currently inside quotes?
    bool quote = false;

    while (i != cmdLine.end() && !isDone)
    {
        // Skip initial whitespace.
        String::skipSpace(i, cmdLine.end());

        // Check for response files.
        bool isResponse = false;
        if (*i == '@')
        {
            isResponse = true;
            String::skipSpace(++i, cmdLine.end());
        }

        String word;

        while (i != cmdLine.end() && (quote || !(*i).isSpace()))
        {
            bool copyChar = true;
            if (!quote)
            {
                // We're not inside quotes.
                if (*i == '\"') // Quote begins.
                {
                    quote = true;
                    copyChar = false;
                }
            }
            else
            {
                // We're inside quotes.
                if (*i == '\"') // Quote ends.
                {
                    auto j = i; j++;
                    if (j != cmdLine.end() && *j == '\"') // Doubles?
                    {
                        // Normal processing, but output only one quote.
                        i = j;
                    }
                    else
                    {
                        quote = false;
                        copyChar = false;
                    }
                }
            }

            if (copyChar)
            {
                word += *i;
            }

            i++;
        }

        // Word has been extracted, examine it.
        if (isResponse) // Response file?
        {
            parseResponseFile(word);
        }
        else if (word == "--") // End of arguments.
        {
            isDone = true;
        }
        else if (!word.empty()) // Make sure there *is *a word.
        {
            d->appendArg(word);
        }
    }
}

void CommandLine::alias(const String &full, const String &alias)
{
    d->aliases[full.toStdString()].push_back(alias);
}

bool CommandLine::isAliasDefinedFor(const String &full) const
{
    const auto &aliases = d.getConst()->aliases;
    return aliases.find(full.toStdString()) != aliases.end();
}

bool CommandLine::matches(const String &full, const String &fullOrAlias) const
{
    if (!full.compareWithoutCase(fullOrAlias))
    {
        // They are, in fact, the same.
        return true;
    }

    Impl::Aliases::const_iterator found = d->aliases.find(full.toStdString());
    if (found != d->aliases.end())
    {
        DE_FOR_EACH_CONST(Impl::ArgumentStrings, i, found->second)
        {
            if (!i->compareWithoutCase(fullOrAlias))
            {
                // Found it among the aliases.
                return true;
            }
        }
    }
    return false;
}

bool CommandLine::execute() const
{
    LOG_AS("CommandLine");

    auto proc = tF::make_ref(d->execute());
    if (!proc)
    {
        LOG_ERROR("Failed to start \"%s\"") << at(0);
        return false;
    }
    LOG_DEBUG("Started detached process %i \"%s\"") << pid_Process(proc) << at(0);
    return true;
}
    
bool CommandLine::executeAndWait(String *output) const
{
    auto proc = tF::make_ref(d->execute());
    if (!proc)
    {
        return false;
    }
    waitForFinished_Process(proc);
    if (output)
    {
        *output = String::take(readOutput_Process(proc));
    }
    return true;
}

iProcess *CommandLine::executeProcess() const
{
    LOG_AS("CommandLine");
    iProcess *proc = d->execute();
    if (proc)
    {
        LOG_MSG("Started process %i \"%s\"") << pid_Process(proc) << at(0);
    }
    return proc;
}

CommandLine &CommandLine::get()
{
    return App::commandLine();
}

CommandLine::ArgWithParams::ArgWithParams() : pos(0)
{}

CommandLine::ArgWithParams::operator dint () const
{
    return pos;
}

dint CommandLine::ArgWithParams::size() const
{
    return params.sizei();
}

} // namespace de
