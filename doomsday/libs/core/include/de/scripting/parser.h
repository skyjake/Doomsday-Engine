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

#ifndef LIBCORE_PARSER_H
#define LIBCORE_PARSER_H

#include "../libcore.h"
#include "tokenbuffer.h"
#include "tokenrange.h"
#include "iparser.h"
#include "scriptlex.h"
#include "operator.h"
#include "expression.h"

namespace de {

class Compound;
class Statement;
class ExpressionStatement;
class PrintStatement;
class IfStatement;
class WhileStatement;
class ForStatement;
class AssignStatement;
class DeleteStatement;
class FunctionStatement;
class ArrayExpression;
class DictionaryExpression;
class OperatorExpression;
class NameExpression;

/**
 * Reads Doomsday script source in text format and outputs the statements
 * of the script into a Script object.
 *
 * @ingroup script
 */
class Parser : public IParser
{
public:
    /// A syntax error is detected during the parsing. Note that the Lex classes
    /// also define syntax errors. @ingroup errors
    DE_ERROR(SyntaxError);

    /// A token is encountered where we don't know what to do with it. @ingroup errors
    DE_SUB_ERROR(SyntaxError, UnexpectedTokenError);

    /// A token is expected, but nothing was found. @ingroup errors
    DE_SUB_ERROR(SyntaxError, MissingTokenError);

    /// A colon is expected but not found. @ingroup errors
    DE_SUB_ERROR(SyntaxError, MissingColonError);

    // Flags for parsing conditional compounds.
    enum CompoundFlag {
        HasCondition = 0x1,
        StayAtClosingStatement = 0x2,
        IgnoreExtraBeforeColon = 0x4
    };
    using CompoundFlags = Flags;

public:
    Parser();
    ~Parser();

    // Implements IParser.
    void parse(const String &input, Script &output);

    void parseCompound(Compound &compound);

    void parseStatement(Compound &compound);

    Expression *parseConditionalCompound(Compound &compound, const CompoundFlags &flags = 0);

    IfStatement *parseIfStatement();

    WhileStatement *parseWhileStatement();

    ForStatement *parseForStatement();

    ExpressionStatement *parseImportStatement();

//    ExpressionStatement *parseExportStatement();

    Statement *parseDeclarationStatement();

    DeleteStatement *parseDeleteStatement();

    FunctionStatement *parseFunctionStatement();

    void parseTryCatchSequence(Compound &compound);

    PrintStatement *parsePrintStatement();

    AssignStatement *parseAssignStatement();

    ExpressionStatement *parseExpressionStatement();

    /// Parse a range of tokens as a comma-separated argument list:
    ArrayExpression *parseList(const TokenRange &range, const char *separator = Token::COMMA,
                               const Flags &flags = Expression::ByValue);

    /// Parse a range of tokens as an operator-based expression.
    Expression *parseExpression(const TokenRange &range, const Flags &flags = Expression::ByValue);

    ArrayExpression *parseArrayExpression(const TokenRange &range);

    DictionaryExpression *parseDictionaryExpression(const TokenRange &range);

    Expression *parseCallExpression(const TokenRange &nameRange, const TokenRange &argumentRange);

    OperatorExpression *parseOperatorExpression(Operator op, const TokenRange &leftSide,
                                                const TokenRange &rightSide,
                                                const Flags &     rightFlags = Expression::ByValue);

    Expression *parseTokenExpression(const TokenRange &range,
                                     const Flags &     flags = Expression::ByValue);

    Operator findLowestOperator(const TokenRange &range, TokenRange &leftSide,
                                TokenRange &rightSide);

protected:
    /// Gets the set of tokens for the next statement.
    /// @return Number of tokens found.
    duint nextStatement();

private:
    ScriptLex _analyzer;

    TokenBuffer _tokens;

    // Range of the current statement. Can be a subrange of the full
    // set of tokens.
    TokenRange _statementRange;
};

} // namespace de

#endif /* LIBCORE_PARSER_H */
