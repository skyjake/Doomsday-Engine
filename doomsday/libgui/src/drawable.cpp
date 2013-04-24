/** @file drawable.cpp  Drawable object with buffers, programs and states.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Drawable"

#include <QMap>

namespace de {

DENG2_PIMPL(Drawable)
{
    typedef QMap<Id, GLBuffer *> Buffers;
    typedef QMap<Id, GLProgram *> Programs;
    typedef QMap<Id, GLState *> States;

    struct BufferConfig {
        GLProgram const *program;
        GLState const *state;

        BufferConfig(GLProgram const *p = 0, GLState const *s = 0)
            : program(p), state(s) {}
    };
    typedef QMap<Id, BufferConfig> BufferConfigs;

    Buffers buffers;
    Programs programs;
    States states;
    BufferConfigs configs;
    GLProgram defaultProgram;

    Instance(Public *i) : Base(i)
    {
        self += defaultProgram;
    }

    ~Instance()
    {
        clear();
    }

    void clear()
    {
        qDeleteAll(buffers.values());
        qDeleteAll(programs.values());
        qDeleteAll(states.values());

        buffers.clear();
        programs.clear();
        states.clear();
        configs.clear();
    }

    void replaceProgram(GLProgram const *src, GLProgram const *dest)
    {
        DENG2_FOR_EACH(BufferConfigs, i, configs)
        {
            if(i.value().program == src)
            {
                i.value().program = dest;
            }
        }
    }

    void replaceState(GLState const *src, GLState const *dest)
    {
        DENG2_FOR_EACH(BufferConfigs, i, configs)
        {
            if(i.value().state == src)
            {
                i.value().state = dest;
            }
        }
    }
};

Drawable::Drawable() : d(new Instance(this))
{}

void Drawable::clear()
{
    d->clear();
}

Drawable::Ids Drawable::allBuffers() const
{
    return d->buffers.keys();
}

Drawable::Ids Drawable::allPrograms() const
{
    Ids ids;
    ids << 0 // default program is always there
        << d->programs.keys();
    return ids;
}

Drawable::Ids Drawable::allStates() const
{
    return d->states.keys();
}

GLBuffer &Drawable::buffer(Id id) const
{
    DENG2_ASSERT(d->buffers.contains(id));
    return *d->buffers[id];
}

GLProgram &Drawable::program(Id id) const
{
    if(!id) return d->defaultProgram;
    DENG2_ASSERT(d->programs.contains(id));
    return *d->programs[id];
}

GLProgram const &Drawable::programForBuffer(Id bufferId) const
{
    DENG2_ASSERT(d->configs.contains(bufferId));
    DENG2_ASSERT(d->configs[bufferId].program != 0);
    return *d->configs[bufferId].program;
}

GLState const *Drawable::stateForBuffer(Id bufferId) const
{
    return d->configs[bufferId].state;
}

GLState &Drawable::state(Id id) const
{
    DENG2_ASSERT(d->states.contains(id));
    return *d->states[id];
}

void Drawable::addBuffer(Id id, GLBuffer *buffer)
{
    removeBuffer(id);

    d->buffers[id] = buffer;
    setProgram(id, d->defaultProgram);
    insert(*buffer, Required);
}

GLProgram &Drawable::addProgram(Id id)
{
    // Program 0 is the default program.
    DENG2_ASSERT(id != 0);

    removeProgram(id);

    GLProgram *p = new GLProgram;
    d->programs[id] = p;
    insert(*p, Required);
    return *p;
}

GLState &Drawable::addState(Id id, GLState const &state)
{
    removeState(id);
    GLState *s = new GLState(state);
    d->states[id] = s;
    return *s;
}

void Drawable::removeBuffer(Id id)
{
    if(d->buffers.contains(id))
    {
        delete d->buffers[id];
        d->buffers.remove(id);
    }
    d->configs.remove(id);
}

void Drawable::removeProgram(Id id)
{
    if(d->programs.contains(id))
    {
        GLProgram *prog = d->programs[id];
        d->replaceProgram(prog, &d->defaultProgram);
        delete prog;
        d->programs.remove(id);
    }
}

void Drawable::removeState(Id id)
{
    if(d->programs.contains(id))
    {
        GLState *st = d->states[id];
        d->replaceState(st, 0);
        delete st;
        d->states.remove(id);
    }
}

void Drawable::setProgram(Id bufferId, GLProgram &program)
{
    d->configs[bufferId].program = &program;
}

void Drawable::setProgram(GLProgram &program)
{
    foreach(Id id, allBuffers())
    {
        setProgram(id, program);
    }
}

void Drawable::setState(Id bufferId, GLState &state)
{
    d->configs[bufferId].state = &state;
}

void Drawable::setState(GLState &state)
{
    foreach(Id id, allBuffers())
    {
        setState(id, state);
    }
}

void Drawable::unsetState(Id bufferId)
{
    d->configs[bufferId].state = 0;
}

void Drawable::unsetState()
{
    foreach(Id id, allBuffers())
    {
        unsetState(id);
    }
}

void Drawable::draw() const
{
    // Ignore the draw request until everything is ready.
    if(!isReady()) return;

    GLProgram const *currentProgram = 0;
    GLState   const *currentState   = 0;

    // Make sure the GL state on the top of the stack is in effect.
    GLState::top().apply();

    DENG2_FOR_EACH_CONST(Instance::Buffers, i, d->buffers)
    {
        Id const id = i.key();

        // Switch the program if necessary.
        GLProgram const &bufProg = programForBuffer(id);
        if(currentProgram != &bufProg)
        {
            if(currentProgram) currentProgram->endUse();

            currentProgram = &bufProg;
            currentProgram->beginUse();
        }

        // If a state has been defined, use it.
        GLState const *bufState = stateForBuffer(id);
        if(bufState && currentState != bufState)
        {
            currentState = bufState;
            currentState->apply();
        }
        else if(!bufState && currentState != 0)
        {
            // Use the current state from the stack.
            currentState = 0;
            GLState::top().apply();
        }

        // Ready to draw.
        i.value()->draw();
    }

    // Cleanup.
    if(currentProgram)
    {
        currentProgram->endUse();
    }
    if(currentState)
    {
        // We messed with the state; restore to what the stack says is current.
        GLState::top().apply();
    }
}

} // namespace de

