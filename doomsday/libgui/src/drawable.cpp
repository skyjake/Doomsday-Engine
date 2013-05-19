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
    typedef QMap<String, Id> Names;

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
    Names bufferNames;
    Names programNames;
    Names stateNames;
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

        bufferNames.clear();
        programNames.clear();
        stateNames.clear();
    }

    Id nextBufferId() const
    {
        Id next = 1;
        // Keys of a QMap are sorted in ascending order.
        if(!buffers.isEmpty()) next = buffers.keys().back() + 1;
        return next;
    }

    Id nextProgramId() const
    {
        Id next = 1;
        // Keys of a QMap are sorted in ascending order.
        if(!programs.isEmpty()) next = programs.keys().back() + 1;
        return next;
    }

    Id nextStateId() const
    {
        Id next = 1;
        // Keys of a QMap are sorted in ascending order.
        if(!states.isEmpty()) next = states.keys().back() + 1;
        return next;
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

    void removeName(Names &names, Id id)
    {
        QMutableMapIterator<String, Id> iter(names);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.value() == id)
            {
                iter.remove();
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

GLBuffer &Drawable::buffer(Name const &bufferName) const
{
    return buffer(bufferId(bufferName));
}

Drawable::Id Drawable::bufferId(Name const &bufferName) const
{
    DENG2_ASSERT(d->bufferNames.contains(bufferName));
    return d->bufferNames[bufferName];
}

GLProgram &Drawable::program(Id id) const
{
    if(!id) return d->defaultProgram;
    DENG2_ASSERT(d->programs.contains(id));
    return *d->programs[id];
}

GLProgram &Drawable::program(Name const &programName) const
{
    return program(programId(programName));
}

Drawable::Id Drawable::programId(Name const &programName) const
{
    DENG2_ASSERT(d->programNames.contains(programName));
    return d->programNames[programName];
}

GLProgram const &Drawable::programForBuffer(Id bufferId) const
{
    DENG2_ASSERT(d->configs.contains(bufferId));
    DENG2_ASSERT(d->configs[bufferId].program != 0);
    return *d->configs[bufferId].program;
}

GLProgram const &Drawable::programForBuffer(Name const &bufferName) const
{
    return programForBuffer(bufferId(bufferName));
}

GLState const *Drawable::stateForBuffer(Id bufferId) const
{
    return d->configs[bufferId].state;
}

GLState const *Drawable::stateForBuffer(Name const &bufferName) const
{
    return stateForBuffer(bufferId(bufferName));
}

GLState &Drawable::state(Id id) const
{
    DENG2_ASSERT(d->states.contains(id));
    return *d->states[id];
}

GLState &Drawable::state(Name const &stateName) const
{
    return state(stateId(stateName));
}

Drawable::Id Drawable::stateId(Name const &stateName) const
{
    DENG2_ASSERT(d->stateNames.contains(stateName));
    return d->stateNames[stateName];
}

void Drawable::addBuffer(Id id, GLBuffer *buffer)
{
    removeBuffer(id);

    d->buffers[id] = buffer;
    setProgram(id, d->defaultProgram);
    insert(*buffer, Required);
}

Drawable::Id Drawable::addBuffer(Name const &bufferName, GLBuffer *buffer)
{
    Id id = d->nextBufferId();
    d->bufferNames.insert(bufferName, id);
    addBuffer(id, buffer);
    return id;
}

Drawable::Id Drawable::addBuffer(GLBuffer *buffer)
{
    Id const id = d->nextBufferId();
    addBuffer(id, buffer);
    return id;
}

Drawable::Id Drawable::addBufferWithNewProgram(GLBuffer *buffer, Name const &programName)
{
    // Take ownership of the buffer.
    Id const bufId = d->nextBufferId();
    addBuffer(bufId, buffer);
    // Assign a new program to the buffer.
    Id const progId = addProgram(programName);
    setProgram(bufId, program(progId));
    return bufId;
}

void Drawable::addBufferWithNewProgram(Id id, GLBuffer *buffer, Name const &programName)
{
    addBuffer(id, buffer);
    addProgram(programName);
    setProgram(id, programName);
}

Drawable::Id Drawable::addBufferWithNewProgram(Name const &bufferName, GLBuffer *buffer,
                                               Name const &programName)
{
    Id const progId = addProgram(programName);
    Id const bufId = addBuffer(bufferName, buffer);
    setProgram(bufId, program(progId));
    return bufId;
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

Drawable::Id Drawable::addProgram(Name const &programName)
{
    Id const id = d->nextProgramId();
    addProgram(id);
    if(!programName.isEmpty())
    {
        d->programNames.insert(programName, id);
    }
    return id;
}

GLState &Drawable::addState(Id id, GLState const &state)
{
    removeState(id);
    GLState *s = new GLState(state);
    d->states[id] = s;
    return *s;
}

Drawable::Id Drawable::addState(Name const &stateName, GLState const &state)
{
    Id const id = d->nextStateId();
    addState(id, state);
    d->stateNames.insert(stateName, id);
    return id;
}

void Drawable::removeBuffer(Id id)
{
    if(d->buffers.contains(id))
    {
        remove(*d->buffers[id]);
        delete d->buffers.take(id);
    }
    d->configs.remove(id);
}

void Drawable::removeProgram(Id id)
{
    if(d->programs.contains(id))
    {
        GLProgram *prog = d->programs[id];
        d->replaceProgram(prog, &d->defaultProgram);
        remove(*prog);
        delete d->programs.take(id);
    }
}

void Drawable::removeState(Id id)
{
    if(d->programs.contains(id))
    {
        GLState *st = d->states[id];
        d->replaceState(st, 0);
        delete d->states.take(id);
    }
}

void Drawable::removeBuffer(Name const &bufferName)
{
    Id const id = bufferId(bufferName);
    removeBuffer(id);
    d->removeName(d->bufferNames, id);
}

void Drawable::removeProgram(Name const &programName)
{
    Id const id = programId(programName);
    removeProgram(id);
    d->removeName(d->programNames, id);
}

void Drawable::removeState(Name const &stateName)
{
    Id const id = stateId(stateName);
    removeState(id);
    d->removeName(d->stateNames, id);
}

void Drawable::setProgram(Id bufferId, GLProgram &program)
{
    d->configs[bufferId].program = &program;
}

void Drawable::setProgram(Id bufferId, Name const &programName)
{
    setProgram(bufferId, program(programName));
}

void Drawable::setProgram(Name const &bufferName, GLProgram &program)
{
    setProgram(bufferId(bufferName), program);
}

void Drawable::setProgram(Name const &bufferName, Name const &programName)
{
    setProgram(bufferId(bufferName), program(programName));
}

void Drawable::setProgram(GLProgram &program)
{
    foreach(Id id, allBuffers())
    {
        setProgram(id, program);
    }
}

void Drawable::setProgram(Name const &programName)
{
    setProgram(program(programName));
}

void Drawable::setState(Id bufferId, GLState &state)
{
    d->configs[bufferId].state = &state;
}

void Drawable::setState(Id bufferId, Name const &stateName)
{
    setState(bufferId, state(stateName));
}

void Drawable::setState(Name const &bufferName, GLState &state)
{
    setState(bufferId(bufferName), state);
}

void Drawable::setState(Name const &bufferName, Name const &stateName)
{
    setState(bufferId(bufferName), state(stateName));
}

void Drawable::setState(GLState &state)
{
    foreach(Id id, allBuffers())
    {
        setState(id, state);
    }
}

void Drawable::setState(Name const &stateName)
{
    setState(state(stateName));
}

void Drawable::unsetState(Id bufferId)
{
    d->configs[bufferId].state = 0;
}

void Drawable::unsetState(Name const &bufferName)
{
    unsetState(bufferId(bufferName));
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

