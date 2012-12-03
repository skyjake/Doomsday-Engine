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

#include "de/Parser"
#include "de/ScriptLex"
#include "de/TokenBuffer"
#include "de/TokenRange"
#include "de/Script"
#include "de/ExpressionStatement"
#include "de/PrintStatement"
#include "de/IfStatement"
#include "de/WhileStatement"
#include "de/ForStatement"
#include "de/FlowStatement"
#include "de/AssignStatement"
#include "de/DeleteStatement"
#include "de/FunctionStatement"
#include "de/TryStatement"
#include "de/CatchStatement"
#include "de/ArrayExpression"
#include "de/DictionaryExpression"
#include "de/ConstantExpression"
#include "de/OperatorExpression"
#include "de/NameExpression"
#include "de/BuiltInExpression"
#include "de/TextValue"
#include "de/NumberValue"
#include "de/Operator"

#include <sstream>

using std::auto_ptr;
using namespace de;

Parser::Parser()
{}

Parser::~Parser()
{}

void Parser::parse(String const &input, Script &output)
{
    // Lexical analyzer for Haw scripts.
    _analyzer = ScriptLex(input);
    
    // Get the tokens of the first statement.
    if(nextStatement() > 0)
    {
        // Parse the bottom-level compound.
        parseCompound(output.compound());
    }

    // We're done, free the remaining tokens.
    _tokens.clear();
}

duint Parser::nextStatement()
{
    duint result = _analyzer.getStatement(_tokens);
    
    // Begin with the whole thing.
    _statementRange = TokenRange(_tokens);

    //std::cout << "Next statement: '" << _statementRange.asText() << "'\n";
    
    return result;
}

void Parser::parseCompound(Compound &compound)
{
    while(_statementRange.size() > 0) 
    {
        if(_statementRange.firstToken().equals(ScriptLex::ELSIF) ||
           _statementRange.firstToken().equals(ScriptLex::ELSE) ||
           _statementRange.firstToken().equals(ScriptLex::CATCH) ||
           (_statementRange.size() == 1 && _statementRange.firstToken().equals(ScriptLex::END)))
        {
            // End of compound.
            break;
        }
        
        // We have a list of tokens, which form a statement.
        // Figure out what it is and generate the correct statement(s)
        // and expressions.
        parseStatement(compound);
    }
}

void Parser::parseStatement(Compound &compound)
{
    DENG2_ASSERT(!_statementRange.empty());
    
    Token const &firstToken = _statementRange.firstToken();

    // Statements with a compound: if, for, while, def.
    if(firstToken.equals(ScriptLex::IF))
    {
        compound.add(parseIfStatement());
        return;
    }
    else if(firstToken.equals(ScriptLex::WHILE))
    {
        compound.add(parseWhileStatement());
        return;
    }
    else if(firstToken.equals(ScriptLex::FOR))
    {
        compound.add(parseForStatement());
        return;
    }
    else if(firstToken.equals(ScriptLex::DEF))
    {
        compound.add(parseFunctionStatement());
        return;
    }
    else if(firstToken.equals(ScriptLex::TRY))
    {
        parseTryCatchSequence(compound);
        return;
    }
    
    // Statements without a compound (must advance to next statement manually).
    if(firstToken.equals(ScriptLex::IMPORT))
    {
        compound.add(parseImportStatement());
    }
    else if(firstToken.equals(ScriptLex::RECORD))
    {
        compound.add(parseDeclarationStatement());
    }
    else if(firstToken.equals(ScriptLex::DEL))
    {
        compound.add(parseDeleteStatement());
    }
    else if(firstToken.equals(ScriptLex::PASS))
    {
        compound.add(new FlowStatement(FlowStatement::PASS));
    }
    else if(firstToken.equals(ScriptLex::CONTINUE))
    {
        compound.add(new FlowStatement(FlowStatement::CONTINUE));
    }
    else if(firstToken.equals(ScriptLex::BREAK))
    {
        // Break may have an expression argument that tells us how many
        // nested compounds to break out of.
        Expression *breakCount = 0;
        if(_statementRange.size() > 1)
        {
            breakCount = parseExpression(_statementRange.startingFrom(1));
        }
        compound.add(new FlowStatement(FlowStatement::BREAK, breakCount));
    }
    else if(firstToken.equals(ScriptLex::RETURN) || firstToken.equals(ScriptLex::THROW))
    {
        Expression *argValue = 0;
        if(_statementRange.size() > 1)
        {
            argValue = parseExpression(_statementRange.startingFrom(1));
        }
        compound.add(new FlowStatement(
            firstToken.equals(ScriptLex::RETURN)? FlowStatement::RETURN : FlowStatement::THROW,
            argValue));
    }
    else if(firstToken.equals(ScriptLex::PRINT))
    {
        compound.add(parsePrintStatement());
    }
    else if(_statementRange.hasBracketless(ScriptLex::ASSIGN) ||
            _statementRange.hasBracketless(ScriptLex::SCOPE_ASSIGN) ||
            _statementRange.hasBracketless(ScriptLex::WEAK_ASSIGN))
    {
        compound.add(parseAssignStatement());
    }
    else if(firstToken.equals(ScriptLex::EXPORT))
    {
        compound.add(parseExportStatement());
    }
    else
    {
        compound.add(parseExpressionStatement());
    }
    
    // We've fully parsed the current set of tokens, get the next statement.
    nextStatement();
}

IfStatement *Parser::parseIfStatement()
{
    // The "end" keyword is necessary in the full form.
    bool expectEnd = !_statementRange.hasBracketless(Token::COLON);
    
    auto_ptr<IfStatement> statement(new IfStatement());
    statement->newBranch();
    statement->setBranchCondition(
        parseConditionalCompound(statement->branchCompound(), 
            HasCondition | StayAtClosingStatement));
    
    while(_statementRange.beginsWith(ScriptLex::ELSIF))
    {
        expectEnd = !_statementRange.hasBracketless(Token::COLON);
        statement->newBranch();
        statement->setBranchCondition(
            parseConditionalCompound(statement->branchCompound(), 
                HasCondition | StayAtClosingStatement));
    }
    
    if(_statementRange.beginsWith(ScriptLex::ELSE))
    {
        expectEnd = !_statementRange.has(Token::COLON);
        parseConditionalCompound(statement->elseCompound(), StayAtClosingStatement);
    }
    
    if(expectEnd) 
    {
        if(_statementRange.size() != 1 || !_statementRange.firstToken().equals(ScriptLex::END))
        {
            throw UnexpectedTokenError("Parser::parseIfStatement", "Expected '" + ScriptLex::END +
                                       "', but got " + _statementRange.firstToken().asText());
        }
        nextStatement();
    }

    return statement.release(); 
}

WhileStatement *Parser::parseWhileStatement()
{
    // "while" expr ":" statement
    // "while" expr "\n" compound
    
    auto_ptr<WhileStatement> statement(new WhileStatement());
    statement->setCondition(parseConditionalCompound(statement->compound(), HasCondition));
    return statement.release();
}

ForStatement *Parser::parseForStatement()
{
    // "for" by-ref-expr "in" expr ":" statement
    // "for" by-ref-expr "in" expr "\n" compound
    
    dint colonPos = _statementRange.find(Token::COLON);
    dint inPos = _statementRange.find(ScriptLex::IN);
    if(inPos < 0 || (colonPos > 0 && inPos > colonPos))
    {
        throw MissingTokenError("Parser::parseForStatement",
            "Expected 'in' to follow " + _statementRange.firstToken().asText());
    }
    
    auto_ptr<Expression> iter(parseExpression(_statementRange.between(1, inPos),
        Expression::ByReference | Expression::NewVariable | Expression::LocalOnly));
    Expression *iterable = parseExpression(_statementRange.between(inPos + 1, colonPos));
    
    auto_ptr<ForStatement> statement(new ForStatement(iter.release(), iterable));

    // Parse the statements of the method.
    parseConditionalCompound(statement->compound(), IgnoreExtraBeforeColon);
    
    return statement.release();
}

ExpressionStatement *Parser::parseImportStatement()
{
    // "import" ["record"] name-expr ["," name-expr]*
 
    if(_statementRange.size() < 2)
    {
        throw MissingTokenError("Parser::parseImportStatement",
            "Expected identifier to follow " + _statementRange.firstToken().asText());
    }
    dint startAt = 1;
    Expression::Flags flags =
            Expression::Import     |
            Expression::NotInScope |
            Expression::LocalOnly;
    if(_statementRange.size() >= 3 && _statementRange.token(1).equals(ScriptLex::RECORD))
    {
        // Take a copy of the imported record instead of referencing it.
        flags |= Expression::ByValue;
        startAt = 2;
    }
    return new ExpressionStatement(parseList(_statementRange.startingFrom(startAt), Token::COMMA, flags));
}

ExpressionStatement *Parser::parseExportStatement()
{
    // "export" name-expr ["," name-expr]*

    if(_statementRange.size() < 2)
    {
        throw MissingTokenError("Parser::parseExportStatement",
            "Expected identifier to follow " + _statementRange.firstToken().asText());
    }

    return new ExpressionStatement(parseList(_statementRange.startingFrom(1), Token::COMMA,
                                             Expression::Export | Expression::LocalOnly));
}

ExpressionStatement *Parser::parseDeclarationStatement()
{
    // "record" name-expr ["," name-expr]*

    if(_statementRange.size() < 2)
    {
        throw MissingTokenError("Parser::parseDeclarationStatement",
            "Expected identifier to follow " + _statementRange.firstToken().asText());
    }    
    Expression::Flags flags = Expression::LocalOnly | Expression::NewSubrecord;
    return new ExpressionStatement(parseList(_statementRange.startingFrom(1), Token::COMMA, flags));
}

DeleteStatement *Parser::parseDeleteStatement()
{
    // "del" name-expr ["," name-expr]*
    
    if(_statementRange.size() < 2)
    {
        throw MissingTokenError("Parser::parseDeleteStatement",
            "Expected identifier to follow " + _statementRange.firstToken().asText());
    }    
    return new DeleteStatement(parseList(_statementRange.startingFrom(1), Token::COMMA,
                                         Expression::LocalOnly | Expression::ByReference));
}

FunctionStatement *Parser::parseFunctionStatement()
{
    dint pos = _statementRange.find(Token::PARENTHESIS_OPEN);
    if(pos < 0)
    {
        throw MissingTokenError("Parser::parseMethodStatement",
            "Expected arguments for " + _statementRange.firstToken().asText());
    }

    // The function must have a name that is not already in use in the scope.
    auto_ptr<FunctionStatement> statement(new FunctionStatement(
        parseExpression(_statementRange.between(1, pos), 
            Expression::LocalOnly | Expression::ByReference | Expression::NewVariable | Expression::NotInScope)));

    // Collect the argument names.
    TokenRange argRange = _statementRange.between(pos + 1, _statementRange.closingBracket(pos));
    if(!argRange.empty())
    {
        // The arguments are comma-separated.
        TokenRange delim = argRange.undefinedRange();
        while(argRange.getNextDelimited(Token::COMMA, delim))
        {
            if(delim.size() == 1 && delim.firstToken().type() == Token::IDENTIFIER)
            {
                // Just the name of the argument.
                statement->addArgument(delim.firstToken().str());
            }
            else if(delim.size() >= 3 && 
                delim.token(0).type() == Token::IDENTIFIER &&
                delim.token(1).equals(ScriptLex::ASSIGN))
            {
                // Argument with a default value.
                statement->addArgument(delim.firstToken().str(),
                    parseExpression(delim.startingFrom(2)));
            }
            else
            {
                throw UnexpectedTokenError("Parser::parseFunctionStatement",
                    "'" + delim.asText() + "' was unexpected in argument definition at " +
                    argRange.firstToken().asText());
            }
        }
    }

    // Parse the statements of the function.
    parseConditionalCompound(statement->compound(), IgnoreExtraBeforeColon);
    
    return statement.release();
}

void Parser::parseTryCatchSequence(Compound &compound)
{
    // "try" cond-compound catch-compound [catch-compound]*
    // catch-compound: "catch" name-expr ["," ref-name-expr] cond-compound
    
    auto_ptr<TryStatement> tryStat(new TryStatement);
    parseConditionalCompound(tryStat->compound(), StayAtClosingStatement);
    compound.add(tryStat.release());

    // One catch is required.
    if(!_statementRange.firstToken().equals(ScriptLex::CATCH))
    {
        throw UnexpectedTokenError("Parser::parseTryCatchSequence",
            "Expected 'catch', but got " + _statementRange.firstToken().asText());
    }
    CatchStatement *finalCatch = 0;
    bool expectEnd = false;
    while(_statementRange.firstToken().equals(ScriptLex::CATCH))
    {
        dint colon = _statementRange.find(Token::COLON);
        expectEnd = (colon < 0);

        // Parse the arguments.
        auto_ptr<ArrayExpression> args;
        if(_statementRange.size() > 1)
        {
            TokenRange argRange;
            if(colon < 0)
            {
                argRange = _statementRange.startingFrom(1);
            }
            else
            {
                argRange = _statementRange.between(1, colon);
            }
            args.reset(parseList(argRange, Token::COMMA,
                Expression::ByReference | Expression::LocalOnly | Expression::NewVariable));
        }

        auto_ptr<CatchStatement> catchStat(new CatchStatement(args.release()));
        parseConditionalCompound(catchStat->compound(),
            StayAtClosingStatement | IgnoreExtraBeforeColon);

        // The final catch will be flagged.
        finalCatch = catchStat.get();

        // Add it to the compound.
        compound.add(catchStat.release());
    }    
    finalCatch->flags |= CatchStatement::FinalCompound;
    if(expectEnd)
    {
        if(!_statementRange.firstToken().equals(ScriptLex::END))
        {
            throw UnexpectedTokenError("Parser::parseTryCatchSequence",
            "Expected 'end', but got " + _statementRange.firstToken().asText());
        }
        nextStatement();
    }
}

PrintStatement *Parser::parsePrintStatement()
{    
    ArrayExpression *args = 0;
    if(_statementRange.size() == 1) // Just the keyword.
    {
        args = new ArrayExpression();
    }
    else
    {
        // Parse the arguments of the print statement.
        args = parseList(_statementRange.startingFrom(1));
    }
    return new PrintStatement(args);    
}

AssignStatement *Parser::parseAssignStatement()
{
    Expression::Flags flags = Expression::NewVariable | Expression::ByReference | Expression::LocalOnly;
    
    /// "export" will export the newly assigned variable.
    if(_statementRange.firstToken().equals(ScriptLex::EXPORT))
    {
        flags |= Expression::Export;
        _statementRange = _statementRange.startingFrom(1);
    }

    /// "const" makes read-only variables.
    if(_statementRange.firstToken().equals(ScriptLex::CONST))
    {
        flags |= Expression::ReadOnly;
        _statementRange = _statementRange.startingFrom(1);
    }
    
    dint pos = _statementRange.find(ScriptLex::ASSIGN);
    if(pos < 0)
    {
        flags &= ~Expression::LocalOnly;
        pos = _statementRange.find(ScriptLex::SCOPE_ASSIGN);
        if(pos < 0)
        {
            // Must be weak assingment, then.
            pos = _statementRange.find(ScriptLex::WEAK_ASSIGN);
            flags |= Expression::ThrowawayIfInScope;
        }
    }
    
    // Has indices been specified?
    AssignStatement::Indices indices;
    dint nameEndPos = pos;
    dint bracketPos = pos - 1;
    try
    {
        while(_statementRange.token(bracketPos).equals(Token::BRACKET_CLOSE))
        {
            dint startPos = _statementRange.openingBracket(bracketPos);
            nameEndPos = startPos;
            Expression *indexExpr = parseExpression(
                _statementRange.between(startPos + 1, bracketPos));
            indices.push_back(indexExpr);
            bracketPos = nameEndPos - 1;
        }
        
        if(indices.size() > 0 && (flags & Expression::ThrowawayIfInScope))
        {
            throw SyntaxError("Parser::parseAssignStatement",
                "Weak assignment cannot be used with indices");
        }

        auto_ptr<Expression> lValue(parseExpression(_statementRange.endingTo(nameEndPos), flags));
        auto_ptr<Expression> rValue(parseExpression(_statementRange.startingFrom(pos + 1)));

        AssignStatement *st = new AssignStatement(lValue.get(), indices, rValue.get());

        lValue.release();
        rValue.release();
        
        return st;
    }
    catch(Error const &)
    {
        // Cleanup.
        for(AssignStatement::Indices::iterator i = indices.begin(); i != indices.end(); ++i)
        {
            delete *i;
        }
        throw;
    }
}

ExpressionStatement *Parser::parseExpressionStatement()
{
    return new ExpressionStatement(parseExpression(_statementRange));
}

Expression *Parser::parseConditionalCompound(Compound &compound, CompoundFlags const &flags)
{
    // keyword [expr] ":" statement
    // keyword [expr] "\n" compound
    
    TokenRange range = _statementRange;
    
    // See if there is a colon on this line.
    dint colon = range.findBracketless(Token::COLON);
    
    auto_ptr<Expression> condition;
    if(flags.testFlag(HasCondition))
    {
        TokenRange conditionRange = range.between(1, colon);
        if(conditionRange.empty())
        {
            throw MissingTokenError("Parser::parseConditionalCompound",
                "A condition expression was expected after " + range.token(0).asText());
        }
        condition = auto_ptr<Expression>(parseExpression(conditionRange));
    }
    else if(colon > 0 && (colon > 1 && !flags.testFlag(IgnoreExtraBeforeColon)))
    {
        throw UnexpectedTokenError("Parser::parseConditionalCompound",
            range.token(1).asText() + " was unexpected");
    }
    
    if(colon > 0 && colon < dint(range.size()) - 1)
    {
        // The colon is not the last token. There must be a statement 
        // continuing on the same line.
        _statementRange = _statementRange.startingFrom(colon + 1);
        parseStatement(compound);
    }
    else
    {
        nextStatement();
        parseCompound(compound);
        if(!flags.testFlag(StayAtClosingStatement))
        {
            nextStatement();
        }
    }
    return condition.release();
}

ArrayExpression *Parser::parseList(TokenRange const &range, QChar const *separator,
                                   Expression::Flags const &flags)
{
    auto_ptr<ArrayExpression> exp(new ArrayExpression);
    if(range.size() > 0)
    {
        // The arguments are comma-separated.
        TokenRange delim = range.undefinedRange();
        while(range.getNextDelimited(separator, delim))
        {
            exp->add(parseExpression(delim, flags));
        }
    }
    return exp.release();
}

Expression *Parser::parseExpression(TokenRange const &fullRange, Expression::Flags const &flags)
{
    TokenRange range = fullRange;
    
    if(!range.size())
    {
        // Empty expression yields a None value.
        return ConstantExpression::None();
    }
    
    // We can ignore extra parenthesis around the range.
    while(range.firstToken().equals(Token::PARENTHESIS_OPEN) && range.closingBracket(0) == range.size() - 1)
    {
        range = range.shrink(1);
    }

    TokenRange leftSide = range.between(0, 0);
    TokenRange rightSide = leftSide;
    
    // Locate the lowest-ranking operator.
    Operator op = findLowestOperator(range, leftSide, rightSide);
    
    if(op == NONE)
    {
        // This is a constant or a variable reference.
        return parseTokenExpression(range, flags);
    }
    else if(op == ARRAY)
    {
        return parseArrayExpression(range);
    }
    else if(op == DICTIONARY)
    {
        return parseDictionaryExpression(range);
    }
    else if(op == CALL)
    {
        return parseCallExpression(leftSide, rightSide);
    }
    else
    {       
        // Left side is empty with unary operators.
        // The right side inherits the flags of the expression (e.g., name-by-reference).
        return parseOperatorExpression(op, leftSide, rightSide, flags);
    }
}

ArrayExpression *Parser::parseArrayExpression(TokenRange const &range)
{
    if(!range.firstToken().equals(Token::BRACKET_OPEN) ||
        range.closingBracket(0) != range.size() - 1)
    {
        throw MissingTokenError("Parser::parseArrayExpression",
            "Expected brackets for the array expression beginning at " + 
            range.firstToken().asText());
    }    
    return parseList(range.between(1, range.size() - 1));
}

DictionaryExpression *Parser::parseDictionaryExpression(TokenRange const &range)
{
    if(!range.firstToken().equals(Token::CURLY_OPEN) ||
        range.closingBracket(0) != range.size() - 1)
    {
        throw MissingTokenError("Parser::parseDictionaryExpression",
            "Expected brackets for the dictionary expression beginning at " + 
            range.firstToken().asText());
    }
    TokenRange shrunk = range.shrink(1);
    
    auto_ptr<DictionaryExpression> exp(new DictionaryExpression);
    if(shrunk.size() > 0)
    {
        // The arguments are comma-separated.
        TokenRange delim = shrunk.undefinedRange();
        while(shrunk.getNextDelimited(Token::COMMA, delim))
        {
            dint colonPos = delim.findBracketless(Token::COLON);
            if(colonPos < 0)
            {
                throw MissingTokenError("Parser::parseDictionaryExpression",
                    "Colon is missing from '" + delim.asText() + "' at " +
                    delim.firstToken().asText());
            }
            
            auto_ptr<Expression> key(parseExpression(delim.endingTo(colonPos)));
            Expression *value = parseExpression(delim.startingFrom(colonPos + 1));
            exp->add(key.release(), value);
        }
    }
    return exp.release();
}

Expression *Parser::parseCallExpression(TokenRange const &nameRange, TokenRange const &argumentRange)
{
    //std::cerr << "call name: " << nameRange.asText() << "\n";
    //std::cerr << "call args: " << argumentRange.asText() << "\n";
    
    if(!argumentRange.firstToken().equals(Token::PARENTHESIS_OPEN) ||
         argumentRange.closingBracket(0) < argumentRange.size() - 1)
    {
        throw SyntaxError("Parser::parseCallExpression",
            "Call arguments must be enclosed in parenthesis for " + 
            argumentRange.firstToken().asText());
    }
    
    // Parse the arguments, with possible labels included.
    // The named arguments are evaluated by a dictionary which is always
    // included as the first expression in the array.
    auto_ptr<ArrayExpression> args(new ArrayExpression);
    DictionaryExpression *namedArgs = new DictionaryExpression;
    args->add(namedArgs);
    
    TokenRange argsRange = argumentRange.shrink(1);
    if(!argsRange.empty())
    {
        // The arguments are comma-separated.
        TokenRange delim = argsRange.undefinedRange();
        while(argsRange.getNextDelimited(Token::COMMA, delim))
        {
            if(delim.has(ScriptLex::ASSIGN))
            {
                // A label is included.
                if(delim.size() < 3 || 
                    delim.firstToken().type() != Token::IDENTIFIER ||
                    !delim.token(1).equals(ScriptLex::ASSIGN))
                {
                    throw UnexpectedTokenError("Parser::parseCallExpression",
                        "Labeled argument '" + delim.asText() + "' is malformed");
                }
                // Create a dictionary entry for this.
                Expression *value = parseExpression(delim.startingFrom(2));
                namedArgs->add(new ConstantExpression(new TextValue(delim.firstToken().str())), value);
            }
            else
            {
                // Unlabeled argument.
                args->add(parseExpression(delim));
            }
        }
    }
    
    // Check for some built-in methods, which are usable everywhere.
    if(nameRange.size() == 1)
    {
        BuiltInExpression::Type builtIn = BuiltInExpression::findType(nameRange.firstToken().str());
        if(builtIn != BuiltInExpression::NONE)
        {
            return new BuiltInExpression(builtIn, args.release());
        }
    }
    auto_ptr<Expression> identifier(parseExpression(nameRange, Expression::ByReference));
    return new OperatorExpression(CALL, identifier.release(), args.release());
}

OperatorExpression *Parser::parseOperatorExpression(Operator op, TokenRange const &leftSide, 
    TokenRange const &rightSide, Expression::Flags const &rightFlags)
{
    //std::cerr << "left: " << leftSide.asText() << ", right: " << rightSide.asText() << "\n";
    
    if(leftSide.empty())
    {
        // Must be unary.
        auto_ptr<Expression> operand(parseExpression(rightSide));
        OperatorExpression *x = new OperatorExpression(op, operand.get());
        operand.release();
        return x;
    }
    else
    {
        Expression::Flags leftOpFlags = (leftOperandByReference(op)?
                                             Expression::ByReference : Expression::ByValue);

        Expression::Flags rightOpFlags = rightFlags;
        if(op != MEMBER) rightOpFlags &= ~Expression::ByReference;

        // Binary operation.
        auto_ptr<Expression> leftOperand(parseExpression(leftSide, leftOpFlags));
        auto_ptr<Expression> rightOperand(op == SLICE? parseList(rightSide, Token::COLON) :
            parseExpression(rightSide, rightOpFlags));
        OperatorExpression *x = new OperatorExpression(op, leftOperand.get(), rightOperand.get());
        x->setFlags(rightFlags); // original flags
        rightOperand.release();
        leftOperand.release();
        return x;
    }
}

Expression *Parser::parseTokenExpression(TokenRange const &range, Expression::Flags const &flags)
{
    if(!range.size())
    {
        throw MissingTokenError("Parser::parseTokenExpression",
            "Expected tokens, but got nothing -- near " +
            range.buffer().at(range.tokenIndex(0)).asText());
    }

    Token const &token = range.token(0);

    if(token.type() == Token::KEYWORD)
    {
        if(token.equals(ScriptLex::T_TRUE))
        {
            return ConstantExpression::True();
        }
        else if(token.equals(ScriptLex::T_FALSE))
        {
            return ConstantExpression::False();
        }
        else if(token.equals(ScriptLex::NONE))
        {
            return ConstantExpression::None();
        }
        else if(token.equals(ScriptLex::PI))
        {
            return ConstantExpression::Pi();
        }
    }

    switch(token.type())
    {
    case Token::IDENTIFIER:
        if(range.size() == 1)
        {
            return new NameExpression(range.token(0).str(), flags);
        }
        else
        {
            throw UnexpectedTokenError("Parser::parseTokenExpression", 
                "Unexpected token " + range.token(1).asText());
        }
        
    case Token::LITERAL_STRING_APOSTROPHE:
    case Token::LITERAL_STRING_QUOTED:
    case Token::LITERAL_STRING_LONG:
        return new ConstantExpression(
            new TextValue(ScriptLex::unescapeStringToken(token)));

    case Token::LITERAL_NUMBER:
        return new ConstantExpression(
            new NumberValue(ScriptLex::tokenToNumber(token)));
        
    default:
        throw UnexpectedTokenError("Parser::parseTokenExpression",
            token.asText() + " which was identified as " +
            Token::typeToText(token.type()) + " was unexpected");
    }
}

Operator Parser::findLowestOperator(TokenRange const &range, TokenRange &leftSide, TokenRange &rightSide)
{
    enum {
        MAX_RANK         = 0x7fffffff,
        RANK_MEMBER      = 13,
        RANK_CALL        = 14,
        RANK_INDEX       = 14,
        RANK_SLICE       = 14,
        RANK_DOT         = 15,
        RANK_ARRAY       = MAX_RANK - 1,
        RANK_DICTIONARY  = RANK_ARRAY,
        RANK_PARENTHESIS = MAX_RANK - 1
    };
    enum Direction {
        LEFT_TO_RIGHT,
        RIGHT_TO_LEFT
    };

    Operator previousOp = NONE;
    Operator lowestOp = NONE;
    int lowestRank = MAX_RANK;

    for(duint i = 0, continueFrom = 0; i < range.size(); i = continueFrom)
    {
        continueFrom = i + 1;
        
        int rank = MAX_RANK;
        Operator op = NONE;
        Direction direction = LEFT_TO_RIGHT;
        
        Token const &token = range.token(i);
        
        if(token.equals(Token::PARENTHESIS_OPEN))
        {
            continueFrom = range.closingBracket(i) + 1;
            if((previousOp == NONE || previousOp == INDEX || previousOp == SLICE ||
                previousOp == PARENTHESIS || previousOp == CALL) && i > 0)
            {
                // The previous token was not an operator, but there 
                // was something before this one. It must be a function
                // call.
                op = CALL;
                rank = RANK_CALL;
            }
            else
            {
                // It's parenthesis. Skip past it.
                op = PARENTHESIS;
                rank = RANK_PARENTHESIS;
            }
        }
        else if(token.equals(Token::BRACKET_OPEN))
        {
            continueFrom = range.closingBracket(i) + 1;
            if((previousOp == NONE || previousOp == PARENTHESIS ||
                previousOp == INDEX || previousOp == SLICE || previousOp == CALL) && i > 0)
            {
                if(range.between(i + 1, continueFrom - 1).has(Token::COLON))
                {
                    op = SLICE;
                    rank = RANK_SLICE;
                }
                else
                {
                    op = INDEX;
                    rank = RANK_INDEX;
                }
            }
            else
            {
                op = ARRAY;
                rank = RANK_ARRAY;
            }
        }
        else if(token.equals(Token::CURLY_OPEN))
        {
            continueFrom = range.closingBracket(i) + 1;
            op = DICTIONARY;
            rank = RANK_DICTIONARY;
        }
        else
        {
            const struct {
                char const *token;
                Operator op;
                int rank;
                Direction direction;
            } rankings[] = { // Ascending order.
                { "+=",     PLUS_ASSIGN,        0,          RIGHT_TO_LEFT },
                { "-=",     MINUS_ASSIGN,       0,          RIGHT_TO_LEFT },
                { "*=",     MULTIPLY_ASSIGN,    0,          RIGHT_TO_LEFT },
                { "/=",     DIVIDE_ASSIGN,      0,          RIGHT_TO_LEFT },
                { "%=",     MODULO_ASSIGN,      0,          RIGHT_TO_LEFT },
                { "not",    NOT,                3,          RIGHT_TO_LEFT },
                { "in",     IN,                 4,          LEFT_TO_RIGHT },
                { "==",     EQUAL,              5,          LEFT_TO_RIGHT },
                { "!=",     NOT_EQUAL,          5,          LEFT_TO_RIGHT },
                { "<",      LESS,               6,          LEFT_TO_RIGHT },
                { ">",      GREATER,            6,          LEFT_TO_RIGHT },
                { "<=",     LEQUAL,             6,          LEFT_TO_RIGHT },
                { ">=",     GEQUAL,             6,          LEFT_TO_RIGHT },
                { "+",      PLUS,               9,          LEFT_TO_RIGHT },
                { "-",      MINUS,              9,          LEFT_TO_RIGHT },
                { "*",      MULTIPLY,           10,         LEFT_TO_RIGHT },
                { "/",      DIVIDE,             10,         LEFT_TO_RIGHT },
                { "%",      MODULO,             10,         LEFT_TO_RIGHT },
                { ".",      DOT,                RANK_DOT,   LEFT_TO_RIGHT },
                { 0,        NONE,               MAX_RANK,   LEFT_TO_RIGHT }
            };            
            
            // Operator precedence:
            // .
            // function call
            // member
            // []
            // ()
            // * / %
            // + -
            // & | ^
            // << >>
            // < > <= >= 
            // == !=
            // in
            // not
            // and
            // or
         
            // Check the rankings table.
            for(int k = 0; rankings[k].token; ++k)
            {
                if(token.equals(String(rankings[k].token)))
                {
                    op = rankings[k].op;
                    rank = rankings[k].rank;
                    direction = rankings[k].direction;
                    
                    if(op == DOT) // && previousOp == NONE)
                    {
                        op = MEMBER;
                        rank = RANK_MEMBER;
                        direction = LEFT_TO_RIGHT;
                    }
                    else if(!i || (previousOp != NONE && previousOp != PARENTHESIS &&
                        previousOp != CALL && previousOp != INDEX && previousOp != SLICE &&
                        previousOp != ARRAY && previousOp != DICTIONARY))
                    {
                        // There already was an operator before this one.
                        // Must be unary?
                        if(op == PLUS || op == MINUS)
                        {
                            rank += 100;
                            //std::cerr << "Rank boost for " << rankings[k].token << "\n";
                        }
                    }
                    break;
                }
            }
        }
        
        if(op != NONE && 
            ((direction == LEFT_TO_RIGHT && rank <= lowestRank) ||
             (direction == RIGHT_TO_LEFT && rank < lowestRank)))
        {
            lowestOp = op;
            lowestRank = rank;
            leftSide = range.endingTo(i);
            if(op == INDEX || op == SLICE)
            {
                rightSide = range.between(i + 1, continueFrom - 1);
            }
            else
            {
                rightSide = range.startingFrom(op == CALL || op == ARRAY || 
                    op == DICTIONARY? i : i + 1);
            }
        }
        
        previousOp = op;       
    }
    
    return lowestOp;
}
