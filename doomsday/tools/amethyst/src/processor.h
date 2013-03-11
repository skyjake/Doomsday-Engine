/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef __AMETHYST_PROCESSOR_H__
#define __AMETHYST_PROCESSOR_H__

#include "source.h"
#include "macro.h"
#include "structurecounter.h"
#include "command.h"
#include "commandruleset.h"
#include "ruleset.h"
#include "stringlist.h"
#include "outputcontext.h"
#include "schedule.h"
#include "callstack.h"

#include <QStringList>
#include <QTextStream>

// Processor mode flags.
#define PMF_DUMP_SHARDS     0x1
#define PMF_DUMP_GEMS       0x2
#define PMF_DUMP_SCHEDULE   0x4
#define PMF_STRUCTURED      0x8

class Processor
{
public:
    enum PartialPrintMode { Before, After };

    Processor();
    ~Processor();

    // Accessors.
    void setSourceName(const String& name) { _sourceFileName = name; }

    // User interface.
    void init(QTextStream& input, QTextStream& output);
    bool compile(QTextStream& input, QTextStream& output);
    int setMode(int setFlags = 0, int clearFlags = 0);
    void define(const String& name);
    void addIncludePath(const String& path);
    String locateInclude(const String& fileName);
    Macro& macros() { return _macros; }

    // Parser.
    bool getToken(String &token, bool require = false);
    bool getTokenOrBlank(String &token);
    void pushToken(const String& token);
    void useSource(Source *src);
    void parseInput();
    bool parseStatement(Shard *parent, bool expectClose = true);
    bool parseAt(Shard *parent);
    bool parseBlock(Shard *parent);
    bool parseSimpleBlock(Shard *parent);
    bool parseVerbatim(Shard *parent);

    // Output generator.
    void generateOutput();
    void grindCommand(Command *command, Gem *parent,
        const GemClass &gemClass, bool inheritLength = false);
    void grindShard(Shard *shard, Gem *parent,
        const GemClass &gemClass, bool inheritLength = false);
    
    OutputContext *processTable(Gem *table, OutputContext *host);
    OutputContext *processDefinitionList(Gem *defList, OutputContext *host);
    OutputContext *processList(Gem *list, OutputContext *host);
    OutputContext *processTitle(Gem *title, OutputContext *host);
    OutputContext *processIndent(Gem *ind, OutputContext *host);
    OutputContext *processContents(Gem *contents, OutputContext *host);
    OutputContext *process(Gem *gem, OutputContext *ctx = 0);
    
    void print(Gem *gem, OutputContext *ctx);
    void partialPrint(PartialPrintMode mode, Gem *gem, OutputContext *ctx);
    
    // Messages.
    void warning(const char *format, ...);
    void error(const char *format, ...);

private:
    Source *_in, _sources;
    Shard _root;
    StructureCounter _counter;
    RuleSet _rules;
    CommandRuleSet _commands;
    CallStack _callStack;
    Macro _macros;
    Schedule _schedule;
    QTextStream* _out;
    int  _modeFlags;
    String _outputType;
    String _sourceFileName;
    StringList _defines;
    QStringList _includeDirs;

    void dumpRoot(Shard *at, int level = 0);
    void dumpGems(Gem *at, int level = 0);
};

#endif
