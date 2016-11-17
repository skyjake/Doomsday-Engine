/** @file udmfparser.h  UDMF parser.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/TokenRange>
#include <QHash>
#include <functional>

/**
 * UMDF parser.
 *
 * Reads input text and makes callbacks for each parsed block.
 */
class UDMFParser
{
public:
    typedef QHash<de::String, QVariant> Block;
    typedef std::function<void (de::String const &, QVariant const &)> AssignmentFunc;
    typedef std::function<void (de::String const &, Block const &)> BlockFunc;

    DENG2_ERROR(SyntaxError);

public:
    UDMFParser();

    void setGlobalAssignmentHandler(AssignmentFunc func);
    void setBlockHandler(BlockFunc func);

    Block const &globals() const;

    void parse(de::String const &input);

protected:
    de::dsize nextExpression();
    void parseBlock(Block &block);
    void parseAssignment(Block &block);

private:
    AssignmentFunc _assignmentHandler;
    BlockFunc _blockHandler;
    UDMFLex _analyzer;
    de::TokenBuffer _tokens;
    de::TokenRange _range;
    Block _globals;
};

#endif // IMPORTUDMF_UDMFPARSER_H
