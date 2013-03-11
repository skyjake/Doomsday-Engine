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

#include "schedule.h"
#include "uniquelist.h"
#include "processor.h"
#include "utils.h"
#include <QStringList>
#include <QDebug>

Schedule::Schedule(Processor& proc) : _proc(proc)
{
    _contextRoot.setRoot();
    _relationRoot.setRoot();
}

void Schedule::clear()
{
    _contextRoot.destroy();
    _relationRoot.destroy();
}

OutputContext *Schedule::newContext(OutputContext *initializer)
{
    OutputContext *ctx;

    if(initializer) 
        ctx = new OutputContext(*initializer);
    else
        ctx = new OutputContext;
    _contextRoot.addBefore(ctx);
    return ctx;
}

void Schedule::link(OutputContext *source, OutputContext *target)
{
    _relationRoot.addBefore(new ContextRelation(source, target));
}

ContextRelation *Schedule::list(ListType type, OutputContext *ctx)
{
    ContextRelation *list = new ContextRelation;

    // Make it a root.
    list->setRoot();
    for(ContextRelation *it = _relationRoot.next(); !it->isRoot();
        it = it->next())
    {
        if((type == Preceding && it->target() == ctx) || (type == Following && it->source() == ctx))
            // Make a copy of this relation.
            list->addBefore(new ContextRelation(*it));
    }
    return list;
}

void Schedule::dumpContexts()
{
    ContextRelation *it;

    qDebug() << "SCHEDULE DUMP:";

    QString dump;
    QTextStream out(&dump);

    for(OutputContext *ctx = _contextRoot.next(); !ctx->isRoot(); ctx = ctx->next())
    {
        out << ctx << " (left:" << ctx->leftEdge()
            << ", right: " << ctx->rightEdge() << ")";
        QScopedPointer<ContextRelation> prec(list(Preceding, ctx));
        QScopedPointer<ContextRelation> fol(list(Following, ctx));
        out << "\n  Preceded by: ";
        for(it = prec->next(); !it->isRoot(); it = it->next())
            out << it->source() << " ";
        out << "\n  Followed by: ";
        for(it = fol->next(); !it->isRoot(); it = it->next())
            out << it->target() << " ";
        out << "\n  Text: `" << ctx->output() << "'";

        qDebug() << dump.toLatin1().data();
        dump.clear();
    }
}

/**
 * Returns true if the state was advanced.
 */
bool Schedule::advance(OutputState &state)
{
    OutputState *s, *next;
    ContextRelation *r;
    UniquePtrList candidates, *it;
    bool wasAdvanced = false;
    
    // Collect the list of candidates.
    candidates.setRoot();
    for(s = state.next(); !s->isRoot(); s = s->next())
    {
        // If this state is done, compile a list of possible candidates
        // that will take over if all their predecessors are done.
        if(s->isDone())
        {
            ContextRelation *fol = list(Following, s->context());
            // If no followers, remove the state.
            if(fol->isListEmpty())
                s->mark();
            else for(r = fol->next(); !r->isRoot(); r = r->next())
                candidates.add(r->target());
            delete fol;
        }
    }

    // Check if we can advance any of the states.
    for(it = candidates.next(); !it->isRoot(); it = it->next())
    {
        OutputContext *ctx = (OutputContext*) it->get();
        ContextRelation *prec = list(Preceding, ctx);
        bool takeOver = true;
        // Check if all preceding states are done.
        for(r = prec->next(); takeOver && !r->isRoot(); r = r->next())
        {
            // If it isn't found, the context hasn't yet entered the state.
            s = state.findContext(r->source());
            if(!s || !s->isDone()) takeOver = false;
        }
        // Can we take over?
        // All states taken over are marked for deletion and deleted
        // after the new states have been created.
        if(takeOver)
        {
            wasAdvanced = true;
            // Mark the preceding states for deletion.
            for(r = prec->next(); !r->isRoot(); r = r->next())
                state.findContext(r->source())->mark();
            // Add the candidate context in front of the first
            // preceding one.
            state.findContext(prec->next()->source())->
                addBefore(new OutputState(ctx));
        }
        delete prec;
    }

    // Candidates have been added, delete the marked states.
    for(s = state.next(); !s->isRoot(); s = next)
    {
        next = s->next();
        if(s->isMarked()) delete s->remove();
    }
    return wasAdvanced;
}

/**
 * Renders the schedule's contents to a text stream.
 *
 * @param structuredOutput Output will be structured and padded with spaces and
 *                         newlines for alignment and breaks. Used with
 *                         fixed-width character output.
 */
void Schedule::render(QTextStream &os, bool structuredOutput)
{
    OutputState state;
    
    if(_contextRoot.isListEmpty()) return; // Nothing to print.

    // Initialize the state.
    state.addBefore(new OutputState(_contextRoot.next()));

    QStringList completedLines;
    String line;
    String linePrefix;

    // Rendering of lines will continue until all contexts have been
    // completed.
    while(!state.allDone())
    {
        OutputState *s, *next;
        if(structuredOutput) line = "";

        // A part of the line is taken from all of the current contexts.
        for(s = state.next(); !s->isRoot(); s = next)
        {
            next = s->next();
            if(structuredOutput)
            {
                // ctxLine will be exactly as long as the context is wide.
                String ctxLine = s->filledLine(completedLines);

                // Is there a need to print padding?
                int prevEdge = 0;
                if(!s->prev()->isRoot())
                    prevEdge = s->prev()->context()->rightEdge() + 1;
                for(; prevEdge < s->context()->leftEdge(); prevEdge++)
                    line += " ";

                // This context's contribution to the line.
                line += ctxLine;
            }
            else
            {
                // The lines are printed directly to the completedLines list.
                s->rawOutput(line, linePrefix, completedLines);

                if(advance(state))
                    break;
            }
        }
        if(structuredOutput)
        {
            // Now the line can be printed.
            completedLines << trimRight(line);
        }

        // Advance as long as possible.
        while(advance(state)) {}
    }

    if(!structuredOutput)
    {
        // Finish up with the current line.
        completedLines << line;
    }

    bool useCarriageReturns = (_proc.macros().find("CR_NL") != 0);

    // Print out the completed lines to the output stream.
    foreach(QString s, completedLines)
    {
        if(!structuredOutput)
        {
            // Filter out any remaining control characters.
            s.replace(QChar(OutputContext::CtrlAnchor), "");
        }

        line = s + "\n";
        if(useCarriageReturns)
        {
            line.replace("\n", "\r\n");
        }
        os << line;
    }
}
