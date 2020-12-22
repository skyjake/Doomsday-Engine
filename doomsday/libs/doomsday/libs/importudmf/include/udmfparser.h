/** @file udmfparser.h  UDMF parser.
 *
 * @authors Copyright (c) 2016-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef IMPORTUDMF_UDMFPARSER_H
#define IMPORTUDMF_UDMFPARSER_H

#include "udmflex.h"
#include <de/scripting/tokenrange.h>
#include <de/hash.h>
#include <de/value.h>
#include <functional>

/**
 * UMDF parser.
 *
 * Reads input text and makes callbacks for each parsed block. The parsed contents are
 * not kept in memory.
 */
class UDMFParser
{
public:
    typedef de::Hash<de::String, std::shared_ptr<de::Value>> Block;
    typedef std::function<void (const de::String &, const de::Value &)> AssignmentFunc;
    typedef std::function<void (const de::String &, const Block &)> BlockFunc;

    DE_ERROR(SyntaxError);

public:
    UDMFParser();

    void setGlobalAssignmentHandler(AssignmentFunc func);
    void setBlockHandler(BlockFunc func);

    const Block &globals() const;

    /**
     * Parse UDMF source and make callbacks for global assignments and blocks while
     * parsing.
     *
     * @param input  UDMF source text.
     *
     * @throws SyntaxError  UDMF source text has a syntax error.
     */
    void parse(const de::String &input);

protected:
    de::dsize nextFragment();
    void parseBlock(Block &block);
    void parseAssignment(Block &block);

private:
    AssignmentFunc  _assignmentHandler;
    BlockFunc       _blockHandler;
    Block           _globals;
    UDMFLex         _analyzer;
    de::TokenBuffer _tokens;
    de::TokenRange  _range;
};

#endif // IMPORTUDMF_UDMFPARSER_H
