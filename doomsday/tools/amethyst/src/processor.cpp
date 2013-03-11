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

#include "processor.h"
#include "exception.h"
#include "defs.h"
#include <QDir>
#include <QDebug>

#define DEFAULT_RIGHT_MARGIN  71 // Used if right edge not specified.

Processor::Processor() : _schedule(*this)
{
    _macros.setRoot();
    _modeFlags = 0;

    // Default include paths.
#ifdef AME_LIBDIR
    addIncludePath(AME_LIBDIR);
#endif
}

Processor::~Processor()
{}

void Processor::warning(const char *format, ...)
{
    char buf[1024];
    va_list args;

    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);
    
    qWarning() << Exception(String("Warning: ") + buf, _in->fileName(), _in->lineNumber());
}

void Processor::error(const char *format, ...)
{
    char buf[1024];
    va_list args;

    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);

    throw Exception(buf, _in->fileName(), _in->lineNumber());
}

void Processor::useSource(Source *src)
{
    _sources.addAfter(_in = src);
}

int Processor::setMode(int setFlags, int clearFlags)
{
    _modeFlags |= setFlags;
    _modeFlags &= ~clearFlags;
    return _modeFlags;
}

/**
 * Initializes the state of the processor for compiling a new file.
 */
void Processor::init(QTextStream &input, QTextStream &output)
{
    _sources.destroy();
    useSource(new Source(input));
    _in->setFileName(_sourceFileName);
    _out = &output;

    _root.clear();
    _counter.reset();
    _rules.clear();
    _commands.initStandardRules();
    _schedule.clear();
    _macros.destroy();

    // Create macros for the defines.
    for(StringList *s = _defines.next(); !s->isRoot(); s = s->next())
        _macros.addAfter(new Macro(s->get()));
}

bool Processor::getToken(String &token, bool require)
{
    if(require) 
    {
        _in->mustGetToken(token);
        return true;
    }
    return _in->getToken(token);
}

bool Processor::getTokenOrBlank(String &token)
{
    return _in->getTokenOrBlank(token);
}

void Processor::pushToken(const String& token)
{
    _in->pushToken(token);
}

/**
 * Anything until the next } is collected into an Token. @ is the escape
 * character.
 */
bool Processor::parseVerbatim(Shard *parent)
{
    String str;
    bool escaped = false;
    QChar c;

    while((c = _in->peek()) != EOF)
    {
        if(!escaped)
        {
            if(c == '@') 
            {
                escaped = true;
                _in->ignore();
                continue;
            }
            if(c != '}') str += c;
            _in->ignore();
            if(c == '}') break; // Time to stop.
        }
        else
        {
            _in->ignore();
            escaped = false;
            if(c == '@' || c == '{' || c == '}') str += c;
            // Anything else is ignored.
        }
    }
    // A Block will host the Token.
    Block *block = new Block;
    parent->add(block);
    block->add(new Token(str));
    return true;
}

/**
 * A simple token block only cares about the }, which will end it. All
 * other tokens are convered into Token shards without further processing.
 */
bool Processor::parseSimpleBlock(Shard *parent)
{
    String token;
    Block *block;

    // The first token determines the type of the statement.
    if(!getToken(token)) return false; // No more tokens.
    pushToken(token); // We do want to include it in the block.
    parent->add(block = new Block);
    // Whitespace and breaks are totally ignored.
    while(getToken(token))
    {
        if(token == "}") break;
        block->add(new Token(token));
    }
    return true;
}

/**
 * A block is actually a *token* block. It represents a number of
 * consecutive words. The block ends when a known symbol is encountered,
 * such as }, @ or the block end symbol (line with nothing but whitespace).
 */
bool Processor::parseBlock(Shard *parent)
{
    String token;
    Block *block = new Block;

    parent->add(block);
    while(getTokenOrBlank(token))
    {
        // Does the token block end?
        if(token == "@"
            || token == "{"
            || token == "}"
            || token.isEmpty())
        {
            pushToken(token);
            return true;
        }
        // Add this token to the block.
        block->add(new Token(token));
    }
    return true;
}

/**
 * The @ token has already been read.
 * Syntax: command-name [{...} [{...} ...]]
 */
bool Processor::parseAt(Shard *parent)
{
    String token, commandName;
    Command *command;
    Shard *arg;
    Macro *macro;
    int pos = 0;
    int setModifiers = 0, clearModifiers = 0;

    // First the command name.
    getToken(token, true);
    if(token != "{") 
    {
        commandName = token;
    }
    else
    {
        pushToken(token);
        warning("Missing command name.");
    }
    
    // Any style modifiers?
    if((pos = commandName.indexOf('/')) >= 0)
    {
        foreach(String mod, commandName.mid(pos + 1).split('/'))
        {
            String flag;
            bool set = true;
            if(mod.startsWith('-'))
            {
                // Clear style flag.
                flag = mod.mid(1);
                set = false;
            }
            else
            {
                flag = mod;
            }
            int f = styleForName(flag);
            if(set) setModifiers |= f; else clearModifiers |= f;
        }

        // Delete the modifiers from the command name.
        commandName = commandName.left(pos);
    }

    // Is this a macro?
    if((macro = _macros.find(commandName)))
    {
        command = new Command(macro);
    }
    else
    {
        // Create the command shard.
        command = new Command(_commands.find(commandName));
    }
    parent->add(command);

    // Apply the style modifiers.
    command->rule()->gemClass().modifyStyle(setModifiers, clearModifiers);

    // Conditionals require special handling.
    bool isCond = command->isConditionalCommand(), isDefined;
    bool condFail = true;
    if(isCond) isDefined = command->isName("ifdef");

    // Parse all following arguments. An argument is a single statement.
    while(getTokenOrBlank(token))
    {
        if(token != "{")
        {
            // Not a block; no more arguments.
            pushToken(token);
            break;
        }
        int childCount = command->count();
        if(isCond && childCount)
        {
            // Let's test if the condition passes.
            Token *tok = command->arg();
            condFail = (tok && (_macros.find(tok->token()) != 0) != isDefined);
            if(condFail)
            {
                _in->skipToMatching();
                break;
            }
        }
        // There is an argument. 
        switch(command->rule()->argType(childCount))
        {
        case ArgShard:
            if(!parseStatement(command)) error("Expected an argument.");
            break;

        case ArgBlock:
            // Encase the argument in a shard.
            command->add(arg = new Shard);
            if(!parseSimpleBlock(arg))
                error("Expected an argument of type 'block'.");
            break;

        case ArgToken:
            command->add(arg = new Shard);  // Encase in a shard.
            if(!parseVerbatim(arg))
                error("Expected an argument of type 'verbatim'.");
            break;

        default:
            error("Unknown argument type.");
        }
    }

    if(isCond)
    {
        if(!condFail)
        {
            // The condition passes: contents of the last argument will be used.
            // Make the condition transparent; move children to the parent.
            parent->steal(command->last());
        }

        // The conditional may be followed by @else, let's check.
        getToken(token);
        if(token == "@")
        {
            // Ok, let's see what it is.
            getToken(token);
            if(token == "else")
            {
                getToken(token);
                if(token != "{") error("Expected an argument after else.");
                if(!condFail)
                {
                    // The actual condition succeeded, so just skip the else part.
                    _in->skipToMatching();
                }
                else
                {
                    // The condition failed, which means we must parse else's
                    // contents and use them.
                    parseStatement(parent);
                }
            }
            else
            {
                // Go back, this is not an else.
                pushToken(token);
                pushToken("@");
            }
        }
        else
        {
            // Go back, this is not a command...
            pushToken(token);
        }
    }

    // Mode commands modify the operation of the processor.
    if(command->isModeCommand())
    {
        for(arg = command->first(); arg; arg = arg->next())
        {
            Block *block = (Block*) arg->first();
            if(!block) continue;
            for(Token *it = (Token*) block->first(); it; it = (Token*) it->next())
            {
                String str = it->token();
                if(str == "fill") // Fill between contexts (structured output).
                    _modeFlags |= PMF_STRUCTURED;
                if(str == "!fill")
                    _modeFlags &= ~PMF_STRUCTURED;
            }
        }
    }

    // Source commands operate the source stack.
    if(command->isSourceCommand())
    {
        for(arg = command->first(); arg; arg = arg->next())
        {
            String fileName = locateInclude(((Block*)arg->first())->collect());
            Source *src = new Source(fileName);
            if(!src->isOpen())
            {
                if(command->isName("require"))
                    error("@require: Can't open %s.", fileName.toUtf8().data());
                else
                    warning("@include: Can't open %s.", fileName.toUtf8().data());
            }
            if(src->isOpen())
            {
                useSource(_in = src);
                // Parse the contents of the file.
                while(parseStatement(parent, false)) {}
                _in->remove();
                _in = _sources.next();
            }
            delete src;
        }
    }

    // Macros add an entry to the macros list.
    // @macro{name argtypes}{contents}
    if(command->isMacroCommand()
        && command->count() >= 2)
    {
        Block *ident = (Block*) command->first()->first();
        Token *name = (Token*) ident->first();
        if(!name) error("@macro must have a name.");
        _macros.addAfter(new Macro(name->token(),
            command->last(),
            name->next()? ((Token*)name->next())->token() : ""));
    }

    // Rule commands are processed right after parsing (they don't
    // produce output, anyway; just guide the way other stuff looks).
    if(command->isRuleCommand())
    {
        if(!command->first())
            error("Rules must have at least one argument.");

        // GenerateRule will steal the requirement blocks.      
        _rules.generateRule(command);
    }

    // Preformatting requires that \n => \r. 
    if(command->isName("pre"))
    {
        Token *tok = command->arg();
        if(tok) tok->set(replace(tok->token(), '\n', '\r'));
    }

    // Tidy commands do not generate even shards.
    if(command->isTidy()) delete parent->remove(command);
    return true;
}

/**
 * Returns true if a statement was successfully parsed.
 * A statement is a collection of token blocks and commands. Statements
 * end in } (or the end of file).
 */
bool Processor::parseStatement(Shard *parent, bool expectClose)
{
    String token;
    bool gotToken;
        
    // The first token determines the type of the statement.
    if(!getToken(token)) return false; // No more tokens.
    pushToken(token);

    // Each statement is represented by a shard.
    Shard *statement = parent->add(new Shard);
    
    while((gotToken = getTokenOrBlank(token)))
    {
        if(token == "}") 
        {
            if(parent == &_root) error("Mismatched end of block.");
            break;
        }
        if(token.isEmpty())
        {
            // Blanks create paragraph break commands.
            statement->add(new Command(_commands.find("break")));
        }
        else if(token == "{")   // Sub-statement.
            parseStatement(statement);
        else if(token == "@") 
            parseAt(statement);
        else 
        {
            pushToken(token);
            parseBlock(statement);
        }
    }
    if(expectClose && token != "}")
        error("A block has been left open.");
    // Something was done.
    return true;
}

void Processor::parseInput()
{
    while(parseStatement(&_root, false)) {}
}

void Processor::grindCommand(Command *command, Gem *parent, const GemClass &gemClass, bool inheritLength)
{
    Gem *realParent = parent;
    int idx;

    // Don't let the command's type affect the children.
    GemClass newClass = gemClass + command->gemClass();

    // There must be a Break before breaking commands.
    if(command->isBreaking())
        parent->makeBreak();
    else if(command->isLineBreaking())
        parent->makeBreak(GSF_BREAK_LINE);

    // Independent commands get their own gem to put stuff under.
    if(command->isIndependent())
    {
        // Include the inherited features of the class, but the command's
        // own class is stronger.
        parent = (Gem*) parent->add(new
            Gem(command->gemClass() + gemClass));

        // Explicit lengths are inherited only by commands immediately
        // following.
        if(inheritLength)
            parent->gemClass().length() = gemClass.length();
    }

    if(command->isCall())
    {
        // The command is a macro call.
        _callStack.push(command);
        grindShard(command->macroShard(), parent, gemClass);
        // Must restore the stack to its original state.
        _callStack.pop();
    }
    else if(!_callStack.isEmpty() && (command->isArgCommand()
        || command->isReverseArgCommand()))
    {
        // There must be a caller that provides the arguments.
        Shard *caller = _callStack.pop();
        // The index is one-based.
        int index = command->argCommandIndex();
        if(inheritLength) newClass.length() = gemClass.length();
        grindShard(caller->child(command->isReverseArgCommand()?
            -index : index), parent, newClass, inheritLength);
        // Must restore the stack to its original state.
        _callStack.push(caller);
    }
    else if((idx = _counter.indexForName(command->name())) != CNT_NONE)
    {
        _counter.increment(idx);
        // A gem for the counter.
        Gem *sub;
        parent->add(sub = new Gem);
        sub->add(new Gem(newClass, _counter.text(idx)));
        // Add everything inside the first argument (the title text).
        parent->add(sub = new Gem);
        grindShard(command->first(), sub, newClass);
    }
    else if(command->isListCommand() && command->first()) // Must have an argument.
    {
        Gem *item = 0;
        // Generate gems for each @item command.
        // If there is something before the first @item, it is discarded.
        for(Shard *it = command->first()->first();
            it; it = it->next())
        {
            if(it->type() == Shard::COMMAND)
            {
                Command *cmd = (Command*) it;
                if(cmd->isItemCommand())
                {
                    // Begin a new gem.
                    parent->add(item = new
                        Gem(_commands.find(cmd->name())->gemClass()
                        + gemClass));
                    continue;
                }
            }
            // Do not use the list's style for the children.
            if(item) grindShard(it, item, gemClass);
        }
        // Make sure the list is valid (it has at least one item).
        if(!parent->first()) parent->add(new Gem);
    }
    else if(command->isDefinitionListCommand() && command->first()) // Must have an argument.
    {
        // Each item gets its own gem, and under each one for the 'term'
        // and another for the 'definition'. Again, anything before the
        // first @item is discarded.
        Gem *item = 0, *term = 0, *def = 0;
        for(Shard *it = command->first()->first();
            it; it = it->next())
        {
            if(it->type() == Shard::COMMAND)
            {
                Command *cmd = (Command*) it;
                if(cmd->isItemCommand())
                {
                    // Begin a new definition.
                    parent->add(item = new
                        Gem(_commands.find(cmd->name())->gemClass() + gemClass));
                    // Add the term and def sub-gems.
                    GemClass defClass = gemClass/* + GemClass(GSF_DEFINITION)*/;
                    item->add(term = new Gem(defClass));
                    item->add(def = new Gem);
                    // The term is the first argument of @item.
                    // Apply the Definition style flag automatically.
                    grindShard(cmd->first(), term, defClass);
                    continue;
                }
            }
            // Everything else is interpreted as the definition of the
            // current term.
            if(def) grindShard(it, def, gemClass);
        }
    }
    else if(command->isTableCommand() && command->count() >= 2) // Needs cols and the contents.
    {
        Token *cols = command->arg();
        // The cols are a bunch of tokens, each representing
        // a column width.
        int numCols = 0, colWidth[MAX_COLUMNS], colIdx;
        memset(colWidth, 0, sizeof(colWidth));
        for(; cols; cols = (Token*) cols->next())
        {
            // Ignore everything but tokens.
            if(cols->type() != Shard::TOKEN) continue;
            colWidth[numCols++] = cols->token().toInt();
        }

        // The contents is a statement, where @row separates rows and
        // @tab separates columns. @row can take an argument that describes
        // the line separating the rows (single, double, thick).
        // The contents is always the *last* argument of the @table command.
        colIdx = 0;
        // Begin the first row and its first cell.
        Shard *it;
        Gem *row = (Gem*) parent->add(new Gem(newClass));
        Gem *cell = (Gem*) row->add(new Gem(newClass));
        cell->setWidth(colWidth[0]);
        for(it = command->last()->first(); it; it = it->next())
        {
            if(it->type() == Shard::COMMAND)
            {
                Command *cmd = (Command*) it;
                if(cmd->isName("span"))
                {
                    // Modify the width of the current cell.
                    int num = 2;
                    Token *tok;
                     // A specific number?
                    if((tok = cmd->arg()))
                        num = tok->token().toInt();
                    // Increment the column index and cell width.
                    for(num--; num > 0; num--)
                    {
                        if(++colIdx >= numCols) colIdx = numCols - 1;
                        cell->setWidth(cell->width() + colWidth[colIdx]);
                    }
                    continue;
                }
                if(cmd->isName("tab"))
                {
                    // Begin a new cell.
                    cell = (Gem*) row->add(new Gem(newClass));
                    if(++colIdx >= numCols) colIdx = numCols - 1;
                    cell->setWidth(colWidth[colIdx]);
                    continue;
                }
                if(cmd->isName("row"))
                {
                    // Begin a new row.
                    row = (Gem*) parent->add(new Gem(newClass));
                    // And the new first cell.
                    cell = (Gem*) row->add(new Gem(newClass));
                    cell->setWidth(colWidth[colIdx = 0]);
                    // Check for parameters.
                    if(cmd->hasArg("single"))
                        row->modifyStyle(GSF_SINGLE);
                    else if(cmd->hasArg("double"))
                        row->modifyStyle(GSF_DOUBLE);
                    else if(cmd->hasArg("thick"))
                        row->modifyStyle(GSF_THICK);
                    continue;
                }
            }
            // Then just process it normally (cell contents).
            // It will not be affected by the table's style.
            grindShard(it, cell, gemClass);
        }
    }
    else if(command->isApplyCommand() && command->count() >= 2)
    {
        GemClass modClass = gemClass;
        modClass.setFilter(((Block*)command->first()->first())->collect());
        grindShard(command->last(), parent, gemClass + modClass);
    }
    else if(command->isSetCommand()
        && command->count() >= 2)
    {
        GemClass modClass = gemClass;
        modClass.length().init( (Token*) command->first()->
            first()->first());
        grindShard(command->last(), parent, modClass, true);
    }
    else if(command->isName("contents"))
    {
        // Create two gems as children: the high and low limit.
        Token *high = command->arg(), *low = NULL;
        if(high) low = (Token*) high->next();
        parent->add(new Gem(GemClass(GemClass::GemType(GS_HIGHEST_TITLE
            + (high? high->token().toInt() : 0)))));
        parent->add(new Gem(GemClass(GemClass::GemType(GS_HIGHEST_TITLE
            + (low? low->token().toInt() : GS_LOWEST_TITLE - GS_HIGHEST_TITLE)))));
    }
    else
    {
        for(Shard *it = command->first(); it; it = it->next())
            grindShard(it, parent, newClass);
    }

    // Post breaking inserts a Break gem after the command.
    if(command->isPostBreaking()) realParent->makeBreak();
    if(command->isPostLineBreaking()) realParent->makeBreak(GSF_BREAK_LINE);
}

void Processor::grindShard(Shard *shard, Gem *parent, const GemClass &gemClass, bool inheritLength)
{
    if(!shard) return;

    switch(shard->type())
    {
    case Shard::SHARD:
        // A shard is ground by grinding all of its children.
        for(Shard* it = shard->first(); it; it = it->next())
        {
            // The same parent, mind you.
            grindShard(it, parent, gemClass, inheritLength);
        }
        break;

    case Shard::BLOCK:
        // Blocks are easy: they can only contain Tokens, so they produce
        // a number of text gems.
        for(Shard *it = shard->first(); it; it = it->next())
        {
            Token *tok = (Token*) it;
            Gem* gem = new Gem(gemClass, tok->unEscape());
            parent->add(gem);
        }
        break;

    case Shard::COMMAND:
        grindCommand((Command*)shard, parent, gemClass, inheritLength);
        break;

    default:
        error("Internal error! Unknown shard type.");
    }
}

/**
 * Also advances the context.
 */
void Processor::print(Gem *gem, OutputContext *ctx)
{
    // We can't print control gems.
    if(gem->isControl()
        && !gem->isBreak()
        && !gem->isLineBreak()) return;

    // Normal output.
    String out = _rules.apply(gem);
    if(!out.isEmpty())
    {
        // Print a spacing.
        int spacing = _rules.measure(gem).get(Length::Spacing);
        // Add spacing to the output.
        while(spacing-- > 0) ctx->print("\t"); // A breaking space.
        ctx->print(out);
    }

    // Set the align mode of the context.
    OutputContext::AlignMode mode;
    switch(gem->gemClass().flushMode())
    {
    default:
        mode = OutputContext::AlignLeft;
        break;

    case GemClass::FlushRight:
        mode = OutputContext::AlignRight;
        break;

    case GemClass::FlushCenter:
        mode = OutputContext::AlignCenter;
        break;
    }
    ctx->setAlignMode(mode);
}

void Processor::partialPrint(PartialPrintMode mode, Gem *gem, OutputContext *ctx)
{
    if(mode == Before)
    {
        // Also do the anchor output now.
        String out = _rules.anchorPrependApply(gem);
        if(!out.isEmpty())
        {
            ctx->print(QChar(OutputContext::CtrlAnchorPrepend) + out + QChar(OutputContext::CtrlAnchorPrepend));
        }
        out = _rules.anchorAppendApply(gem);
        if(!out.isEmpty())
        {
            ctx->print(QChar(OutputContext::CtrlAnchorAppend) + out + QChar(OutputContext::CtrlAnchorAppend));
        }
    }

    ctx->print(mode == Before? _rules.preApply(gem)
        : mode == After? _rules.postApply(gem)
        : _rules.apply(gem));
}

OutputContext* Processor::processTitle(Gem *title, OutputContext *host)
{
    // Split into two: the number and the text.
    OutputContext
        *numberCtx = _schedule.newContext(host),
        *textCtx = _schedule.newContext(host);

    // Link the contexts.
    _schedule.link(host, numberCtx);
    _schedule.link(host, textCtx);

    // The number context is the first child.
    numberCtx->startFrom(title->firstGem());

    // And the title is the second.
    textCtx->startFrom(title->lastGem());

    // Calculate how wide the first column must be.
    Length titleLen(_rules.measure(title));
    int spacing = titleLen.get(Length::Spacing);
    int reqWidth = visualSize(_rules.apply(numberCtx->pos())) + spacing;
    // Move the context edges according to the length rules.
    int leftMargin = titleLen.get(Length::LeftMargin);
    int rightMargin = titleLen.get(Length::RightMargin);
    int numberIndent = _rules.measure(title->firstGem()).get(Length::Indent);
    int titleIndent = _rules.measure(title->lastGem()).get(Length::Indent);
    // Make sure there's enough space for the number.
    if(titleIndent - numberIndent < reqWidth)
        titleIndent = numberIndent + reqWidth;
    numberCtx->moveLeftEdge(leftMargin + numberIndent);
    numberCtx->setWidth(titleIndent - numberIndent);
    textCtx->setLeftEdge(numberCtx->rightEdge() + 1);
    textCtx->moveRightEdge(-rightMargin);

    // Process both the number and the text contexts.
    numberCtx = process(0, numberCtx);
    textCtx = process(0, textCtx);

    OutputContext *followCtx = _schedule.newContext(host);
    _schedule.link(numberCtx, followCtx);
    _schedule.link(textCtx, followCtx);
    return followCtx;
}

OutputContext *Processor::processIndent(Gem *ind, OutputContext *host)
{
    OutputContext *indentedCtx = _schedule.newContext(host);

    // Link the contexts.
    _schedule.link(host, indentedCtx);

    // The indented context starts from inside the Indent gem.
    indentedCtx->startFrom(ind);

    // Modify the left edge according to the length rules.
    Length len(_rules.measure(ind));
    indentedCtx->moveLeftEdge(len.get(Length::LeftMargin));
    indentedCtx->moveRightEdge(-len.get(Length::RightMargin));

    indentedCtx = process(ind->firstGem(), indentedCtx);

    OutputContext *followCtx = _schedule.newContext(host);
    _schedule.link(indentedCtx, followCtx);
    return followCtx;
}

OutputContext *Processor::processList(Gem *list, OutputContext *host)
{
    OutputContext *bulletCtx, *textCtx, *midCtx, *followCtx;
    Length listLen(_rules.measure(list));
    int itemSpace = listLen.get(Length::Spacing);
    Gem *item;

    // Cancel if the list contains no items.
    if(!list->firstGem()) return host;

    // Create the first two contexts.
    bulletCtx = _schedule.newContext(host);
    textCtx = _schedule.newContext(host);
    _schedule.link(host, bulletCtx);
    _schedule.link(host, textCtx);

    // Each item needs a pair of contexts.
    for(item = list->firstGem(); item; item = item->nextGem())
    {
        // The bullet context has no position since it's gem has no children.
        bulletCtx->setPos(NULL);
        textCtx->startFrom(item);

        // Get the width of the bullet.
        String bullet = _rules.apply(item);
        int reqWidth = visualSize(bullet)
            + _rules.measure(item).get(Length::Spacing);
        int width = listLen.get(Length::Indent);
        if(width < reqWidth) width = reqWidth;
        bulletCtx->moveLeftEdge(listLen.get(Length::LeftMargin));
        bulletCtx->setWidth(width);
        textCtx->setLeftEdge(bulletCtx->rightEdge() + 1);
        textCtx->moveRightEdge(-listLen.get(Length::RightMargin));

        bulletCtx->print(bullet);
        textCtx = process(0, textCtx);

        // If there is an item following, create the next pair of
        // contexts, along with a mid context.
        if(item->nextGem())
        {
            // The mid context.
            midCtx = _schedule.newContext(host);
            _schedule.link(bulletCtx, midCtx);
            _schedule.link(textCtx, midCtx);

            // Print something...
            for(int i = 0; i < itemSpace; i++) midCtx->print("\n");

            // The next item.
            bulletCtx = _schedule.newContext(host);
            textCtx = _schedule.newContext(host);
            _schedule.link(midCtx, bulletCtx);
            _schedule.link(midCtx, textCtx);
        }
    }

    // And finally, the following context.
    followCtx = _schedule.newContext(host);
    _schedule.link(bulletCtx, followCtx);
    _schedule.link(textCtx, followCtx);
    return followCtx;
}

OutputContext *Processor::processDefinitionList
    (Gem *defList, OutputContext *host)
{
    OutputContext *termCtx, *defCtx, *midCtx;

    // Cancel if the list has no contents.
    if(!defList->firstGem()) return host;

    Length listLen(_rules.measure(defList));
    int leftMargin = listLen.get(Length::LeftMargin);
    int rightMargin = listLen.get(Length::RightMargin);
    int indent = listLen.get(Length::Indent);
    int spacing = listLen.get(Length::Spacing);

    for(Gem *item = defList->firstGem(); item; item = item->nextGem())
    {
        // Create the term context.
        termCtx = _schedule.newContext(host);
        _schedule.link(item->prevGem()? midCtx : host, termCtx);

        // Configure and process.
        termCtx->moveLeftEdge(leftMargin);
        termCtx->moveRightEdge(-rightMargin);
        termCtx->startFrom(item->firstGem());
        termCtx = process(0, termCtx);

        // Then the definition context.
        defCtx = _schedule.newContext(host);
        _schedule.link(termCtx, defCtx);

        // Configure and process.
        defCtx->moveLeftEdge(leftMargin + indent);
        defCtx->moveRightEdge(-rightMargin);
        defCtx->startFrom(item->lastGem());
        defCtx = process(0, defCtx);

        // The middle context, for filling.
        midCtx = _schedule.newContext(host);
        _schedule.link(defCtx, midCtx);

        if(item->nextGem())
        {
            // Print something...
            for(int i = 0; i < spacing; i++) midCtx->print("\n");
        }
    }
    // The last middle context is the following context.
    return midCtx;
}

OutputContext *Processor::processTable(Gem *table, OutputContext *host)
{
    // If there are no rows, abort.
    if(!table->firstGem()) return host;

    OutputContext *midCtx;  // Between rows.
    OutputContext *cells[MAX_COLUMNS], *cell;
    int i, numCells;
    Gem *item;
    Length tableLen(_rules.measure(table));
    int leftMargin = tableLen.get(Length::LeftMargin);
    int rightMargin = tableLen.get(Length::RightMargin);
    int rowSpace = tableLen.get(Length::Spacing);
    int rowWidth, cumulWidth;

    // Create an indented context for the table itself, on which to base
    // all the middle contexts.
    midCtx = _schedule.newContext(host);
    _schedule.link(host, midCtx);
    midCtx->moveLeftEdge(leftMargin);
    midCtx->moveRightEdge(-rightMargin);
    rowWidth = midCtx->width();

    // Process each row.
    for(Gem *row = table->firstGem(); row; row = row->nextGem())
    {
        int cellSpace = _rules.measure(row).get(Length::Spacing);
        int leftPad = cellSpace/2;
        int rightPad = cellSpace - leftPad;

        // Rows get their own PartialPrint.
        partialPrint(Before, row, midCtx);

        // How many cells on this row?
        numCells = row->count();
        if(numCells > MAX_COLUMNS) numCells = MAX_COLUMNS;

        // Create the contexts of the row.
        for(item = row->firstGem(), i = 0, cumulWidth = 0;
            item && i < MAX_COLUMNS;
            item = item->nextGem(), i++)
        {
            cell = _schedule.newContext(midCtx);
            _schedule.link(midCtx, cell);

            // Configure.
            Length itemLen(_rules.measure(item));
            cell->moveLeftEdge((cumulWidth*rowWidth)/100
                + itemLen.get(Length::LeftMargin)
                + leftPad);
            cumulWidth += item->width();
            cell->setRightEdge(midCtx->leftEdge()
                + (cumulWidth*rowWidth)/100 - 1
                - itemLen.get(Length::RightMargin)
                - rightPad);

            cell->startFrom(item);
            cells[i] = process(0, cell);
        }

        // Create the next middle context.
        midCtx = _schedule.newContext(midCtx);

        // Link it to the row's contexts.
        for(i = 0; i < numCells; i++)
            _schedule.link(cells[i], midCtx);

        // Second part of the row's PartialPrint goes to the middle context.
        partialPrint(After, row, midCtx);

        // Print some row space.
        if(row->nextGem())
            for(i = 0; i < rowSpace; i++) midCtx->print("\r");
    }

    // Create the following context.
    OutputContext *followCtx = _schedule.newContext(host);
    _schedule.link(midCtx, followCtx);
    return followCtx;
}

OutputContext *Processor::processContents(Gem *contents, OutputContext *host)
{
    Length conLen(_rules.measure(contents));
    int indent = conLen.get(Length::Indent);
    int spacing = conLen.get(Length::Spacing);
    int leftMargin = conLen.get(Length::LeftMargin);
    int rightMargin = conLen.get(Length::RightMargin);
    int level, minSpace, space;
    OutputContext *mid, *number = 0, *text = 0;
    Gem *browser = ((Gem*)contents->top())->firstGem();
    Gem *tempParent;
    int highest, lowest;
    bool theFirst = true;

    mid = _schedule.newContext(host);
    _schedule.link(host, mid);

    highest = contents->firstGem()->gemType();
    lowest = contents->lastGem()->gemType();

    // Each title gets a pair of contexts.
    for(; browser; browser = browser->nextGem())
    {
        // First child of contents (storage) is the high limit, the last
        // child is the low limit.
        if(browser->gemType() < highest
            || browser->gemType() > lowest) continue;

        mid->moveLeftEdge(leftMargin);
        mid->moveRightEdge(-rightMargin);

        level = browser->gemType() - highest;

        // We need to borrow the children of this one.
        contents->add(tempParent = new Gem(browser->gemClass()));
        tempParent->steal(browser);

        if(theFirst || !(_modeFlags & PMF_STRUCTURED))
        {
            theFirst = false;
            minSpace = visualSize(_rules.apply(tempParent->firstGem()->
                firstGem())) + 1;
        }
        if(minSpace > spacing)
            space = minSpace;
        else
            space = spacing;

        partialPrint(Before, tempParent, mid);

        // Create the contexts.
        number = _schedule.newContext(mid);
        number->moveLeftEdge(level * indent);
        number->setWidth(space);
        number->startFrom(tempParent->firstGem());
        _schedule.link(mid, number);

        text = _schedule.newContext(mid);
        text->setLeftEdge(number->rightEdge() + 1);
        text->startFrom(tempParent->lastGem());
        _schedule.link(mid, text);

        // Process both, under the rules of the contents.
        number = process(0, number);
        text = process(0, text);

        // Create the new middle context.
        mid = _schedule.newContext(host);
        _schedule.link(number, mid);
        _schedule.link(text, mid);

        partialPrint(After, tempParent, mid);

        // Return the children we borrowed.
        browser->steal(tempParent);
        delete contents->remove(tempParent);
    }
    return mid;
}

/**
 * Processing will turn Gems into printable text.
 * If a context has been provided, it has already been configured.
 * The handling of PartialPrint is not good.
 */
OutputContext *Processor::process(Gem *gem, OutputContext *ctx)
{
    Gem *top = gem, *it;
    //bool closePartialForGem = false;

    // If a context hasn't been provided, let's create one.
    if(!ctx)
    {
        // This happens only for the top gem.
        ctx = _schedule.newContext();

        // Configure the context.
        ctx->startFrom(gem);
        Length len = _rules.measure(gem);
        ctx->setLeftEdge(len.get(Length::LeftMargin));
        int rightEdge = len.get(Length::RightMargin);
        if(!rightEdge) rightEdge = DEFAULT_RIGHT_MARGIN;
        ctx->setRightEdge(rightEdge);
    }

    // If no gem has been provided, use the context's current position.
    if(!gem)
    {
        top = ctx->top();
        gem = ctx->pos();
    }

    // Anything that should precede the gem?
    if(top) partialPrint(Before, top, ctx);

    // No gem! Must abort...
    if(!gem) goto abortProcess;

    switch(gem->gemType())
    {
    case GemClass::PartTitle:
    case GemClass::ChapterTitle:
    case GemClass::SectionTitle:
    case GemClass::SubSectionTitle:
    case GemClass::Sub2SectionTitle:
    case GemClass::Sub3SectionTitle:
    case GemClass::Sub4SectionTitle:
        ctx = processTitle(gem, ctx);
        break;

    case GemClass::Contents:
        ctx = processContents(gem, ctx);
        break;

    case GemClass::List:
        ctx = processList(gem, ctx);
        break;

    case GemClass::DefinitionList:
        ctx = processDefinitionList(gem, ctx);
        break;

    case GemClass::Table:
        ctx = processTable(gem, ctx);
        break;

    case GemClass::Indent:
        ctx = processIndent(gem, ctx);
        break;

    default:
        // We'll keep processing until we run out of gems.
        for(it = ctx->pos(); it;)
        {
            if(it->gemType() == GemClass::Gem)
            {
                // Apply any pre/post rules on this gem.
                if(top != it) partialPrint(Before, it, ctx);

                // This is a normal Gem. Just print it.
                print(it, ctx);

                if(top != it) partialPrint(After, it, ctx);

                it = ctx->nextPos();
            }
            else
            {
                // The context will most likely change.
                ctx = process(it, ctx);
                // Don't descend into the gem since it has already
                // been processed.
                it = ctx->nextPos(false);
            }
        }
    }
abortProcess:

    // Anything that should follow the gem?
    //if(closePartialForGem) partialPrint(After, gem, ctx);
    if(top) partialPrint(After, top, ctx);

    // Return a pointer to the current context.
    return ctx;
}

void Processor::generateOutput()
{
    Gem gems;

    // Start by creating Gems out of the Shards that represent the source file.
    grindShard(&_root, &gems, GemClass());
    gems.polish();

    if(_modeFlags & PMF_DUMP_GEMS) dumpGems(&gems);

    // Create the schedule: a collection of output-ready linked contexts.
    process(&gems);

    if(_modeFlags & PMF_DUMP_SCHEDULE) _schedule.dumpContexts();

    // Render the schedule.
    _schedule.render(*_out, (_modeFlags & PMF_STRUCTURED) != 0);
}

bool Processor::compile(QTextStream &input, QTextStream &output)
{
    init(input, output);

    // Parse the input.
    try { parseInput(); }
    catch(Exception ex) // Catch parse errors.
    {
        qWarning() << ex;
        return false;
    }

    if(_modeFlags & PMF_DUMP_SHARDS) dumpRoot(&_root);

    // Generate output.
    try { generateOutput(); }
    catch(Exception ex) // Catch output errors.
    {
        qWarning() << ex;
        return false;
    }
    return true;
}

void Processor::define(const String& name)
{
    if(name.isEmpty()) return;
    _defines.addAfter(new StringList(name));
}

void Processor::addIncludePath(const String& path)
{
    String str = path;
    QChar c;

    if(str.isEmpty()) return;
    c = str[str.size() - 1];
    if(c != '/') str += '/';
    _includeDirs << str;
}

String Processor::locateInclude(const String& fileName)
{
    if(fileFound(fileName)) return fileName;

    // Directories to look in.
    QStringList dirs = _includeDirs;

    QString implicitDir = QFileInfo(_sourceFileName).path();
    if(!implicitDir.isEmpty())
        dirs.prepend(implicitDir + "/");

    foreach(QString it, dirs)
    {
        String tryName = it + fileName;
        if(fileFound(tryName)) return tryName;
    }
    // Not found...
    return fileName;
}

void Processor::dumpRoot(Shard *at, int level)
{
    if(!level) qDebug() << "SHARD DUMP:";

    QString dump;
    QTextStream out(&dump);

    for(int i = 0; i < level; i++) out << "  ";
    switch(at->type())
    {
    case Shard::SHARD:
        out << "Shard";
        break;

    case Shard::TOKEN:
        out << "Token: `" << ((Token*)at)->token() << "'";
        break;

    case Shard::BLOCK:
        out << "Block";
        break;

    case Shard::COMMAND:
        out << "Command: @" << ((Command*)at)->name();
        break;

    case Shard::GEM:
        out << "Gem";
        break;
    }
    qDebug() << dump.toLatin1().data();

    // Dump all children, too.
    for(Shard *it = at->first(); it; it = it->next())
        dumpRoot(it, level + 1);
}

void Processor::dumpGems(Gem *at, int level)
{
    if(!level) qDebug() << "GEM DUMP:";

    QString dump;
    QTextStream out(&dump);

    for(int i = 0; i < level; i++) out << "  ";
    out << at->dump();
    qDebug() << dump.toLatin1().data();

    // Dump all children, too.
    for(Gem *it = at->firstGem(); it; it = it->nextGem())
        dumpGems(it, level + 1);
}
