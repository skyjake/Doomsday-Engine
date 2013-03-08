/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_PARSER_H
#define LIBDENG2_PARSER_H

#include "../libdeng2.h"
#include "../IParser"
#include "../TokenBuffer"
#include "../TokenRange"
#include "../ScriptLex"
#include "../Operator"
#include "../Expression"

#include <QFlags>

namespace de {

class Compound;
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
    DENG2_ERROR(SyntaxError);

    /// A token is encountered where we don't know what to do with it. @ingroup errors
    DENG2_SUB_ERROR(SyntaxError, UnexpectedTokenError);

    /// A token is expected, but nothing was found. @ingroup errors
    DENG2_SUB_ERROR(SyntaxError, MissingTokenError);

    /// A colon is expected but not found. @ingroup errors
    DENG2_SUB_ERROR(SyntaxError, MissingColonError);

    // Flags for parsing conditional compounds.
    enum CompoundFlag
    {
        HasCondition = 0x1,
        StayAtClosingStatement = 0x2,
        IgnoreExtraBeforeColon = 0x4
    };
    Q_DECLARE_FLAGS(CompoundFlags, CompoundFlag)

public:
    Parser();
    ~Parser();

    // Implements IParser.
    void parse(String const &input, Script &output);

    void parseCompound(Compound &compound);

    void parseStatement(Compound &compound);

    Expression *parseConditionalCompound(Compound &compound, CompoundFlags const &flags = 0);

    IfStatement *parseIfStatement();

    WhileStatement *parseWhileStatement();

    ForStatement *parseForStatement();

    ExpressionStatement *parseImportStatement();

    ExpressionStatement *parseExportStatement();

    ExpressionStatement *parseDeclarationStatement();

    DeleteStatement *parseDeleteStatement();

    FunctionStatement *parseFunctionStatement();

    void parseTryCatchSequence(Compound &compound);

    PrintStatement *parsePrintStatement();

    AssignStatement *parseAssignStatement();

    ExpressionStatement *parseExpressionStatement();

    /// Parse a range of tokens as a comma-separated argument list:
    ArrayExpression *parseList(TokenRange const &range,
                               QChar const *separator = Token::COMMA,
                               Expression::Flags const &flags = Expression::ByValue);

    /// Parse a range of tokens as an operator-based expression.
    Expression *parseExpression(TokenRange const &range,
        Expression::Flags const &flags = Expression::ByValue);

    ArrayExpression *parseArrayExpression(TokenRange const &range);

    DictionaryExpression *parseDictionaryExpression(TokenRange const &range);

    Expression *parseCallExpression(TokenRange const &nameRange,
        TokenRange const &argumentRange);

    OperatorExpression *parseOperatorExpression(Operator op,
        TokenRange const &leftSide, TokenRange const &rightSide,
        Expression::Flags const &rightFlags = Expression::ByValue);

    Expression *parseTokenExpression(TokenRange const &range,
        Expression::Flags const &flags = Expression::ByValue);

    Operator findLowestOperator(TokenRange const &range,
        TokenRange &leftSide, TokenRange &rightSide);

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

Q_DECLARE_OPERATORS_FOR_FLAGS(de::Parser::CompoundFlags)

} // namespace de

#endif /* LIBDENG2_PARSER_H */
