/** @file drawable.cpp  Drawable object with buffers, programs and states.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/drawable.h"
#include <de/keymap.h>

namespace de {

DE_PIMPL(Drawable)
{
    typedef KeyMap<Id, std::shared_ptr<GLBuffer>> Buffers;
    typedef KeyMap<Id, GLProgram *> Programs;
    typedef KeyMap<Id, GLState *>   States;
    typedef KeyMap<String, Id>      Names;

    struct BufferConfig {
        const GLProgram *program;
        const GLState *state;

        BufferConfig(const GLProgram *p = nullptr, const GLState *s = nullptr)
            : program(p), state(s) {}
    };
    typedef KeyMap<Id, BufferConfig> BufferConfigs;

    Buffers       buffers;
    Programs      programs;
    States        states;
    Names         bufferNames;
    Names         programNames;
    Names         stateNames;
    BufferConfigs configs;
    GLProgram     defaultProgram;

    Impl(Public *i) : Base(i)
    {
        self() += defaultProgram;
    }

    ~Impl()
    {
        clear();
    }

    void clear()
    {
        programs.deleteAll();
        states.deleteAll();

        defaultProgram.clear();

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
        // Keys of a Map are sorted in ascending order.
        if (!buffers.empty()) next = buffers.rbegin()->first + 1;
        return next;
    }

    Id nextProgramId() const
    {
        Id next = 1;
        // Keys of a Map are sorted in ascending order.
        if (!programs.empty()) next = programs.rbegin()->first + 1;
        return next;
    }

    Id nextStateId() const
    {
        Id next = 1;
        // Keys of a Map are sorted in ascending order.
        if (!states.empty()) next = states.rbegin()->first + 1;
        return next;
    }

    void replaceProgram(const GLProgram *src, const GLProgram *dest)
    {
        DE_FOR_EACH(BufferConfigs, i, configs)
        {
            if (i->second.program == src)
            {
                i->second.program = dest;
            }
        }
    }

    void replaceState(const GLState *src, const GLState *dest)
    {
        DE_FOR_EACH(BufferConfigs, i, configs)
        {
            if (i->second.state == src)
            {
                i->second.state = dest;
            }
        }
    }

    void removeName(Names &names, Id id)
    {
        for (auto i = names.begin(); i != names.end(); )
        {
            if (i->second == id)
            {
                i = names.erase(i);
            }
            else
            {
                ++i;
            }
        }
    }
};

Drawable::Drawable() : d(new Impl(this))
{}

void Drawable::clear()
{
    d->clear();
}

Drawable::Ids Drawable::allBuffers() const
{
    return map<Ids>(d->buffers, [](const Impl::Buffers::value_type &v) { return v.first; });
}

Drawable::Ids Drawable::allPrograms() const
{
    Ids ids;
    ids << 0 // default program is always there
        << map<Ids>(d->programs, [](const Impl::Programs::value_type &v) { return v.first; });
    return ids;
}

Drawable::Ids Drawable::allStates() const
{
    return map<Ids>(d->states, [](const Impl::States::value_type &v) { return v.first; });
}

bool Drawable::hasBuffer(Id id) const
{
    return d->buffers.contains(id);
}

GLBuffer &Drawable::buffer(Id id) const
{
    DE_ASSERT(d->buffers.contains(id));
    return *d->buffers[id];
}

GLBuffer &Drawable::buffer(const Name &bufferName) const
{
    return buffer(bufferId(bufferName));
}

Drawable::Id Drawable::bufferId(const Name &bufferName) const
{
    DE_ASSERT(d->bufferNames.contains(bufferName));
    return d->bufferNames[bufferName];
}

GLProgram &Drawable::program(Id id) const
{
    if (!id) return d->defaultProgram;
    DE_ASSERT(d->programs.contains(id));
    return *d->programs[id];
}

GLProgram &Drawable::program(const Name &programName) const
{
    return program(programId(programName));
}

Drawable::Id Drawable::programId(const Name &programName) const
{
    if (programName.isEmpty()) return 0; // Default program.
    DE_ASSERT(d->programNames.contains(programName));
    return d->programNames[programName];
}

const GLProgram &Drawable::programForBuffer(Id bufferId) const
{
    DE_ASSERT(d->configs.contains(bufferId));
    DE_ASSERT(d->configs[bufferId].program != nullptr);
    return *d->configs[bufferId].program;
}

const GLProgram &Drawable::programForBuffer(const Name &bufferName) const
{
    return programForBuffer(bufferId(bufferName));
}

const GLState *Drawable::stateForBuffer(Id bufferId) const
{
    return d->configs[bufferId].state;
}

const GLState *Drawable::stateForBuffer(const Name &bufferName) const
{
    return stateForBuffer(bufferId(bufferName));
}

GLState &Drawable::state(Id id) const
{
    DE_ASSERT(d->states.contains(id));
    return *d->states[id];
}

GLState &Drawable::state(const Name &stateName) const
{
    return state(stateId(stateName));
}

Drawable::Id Drawable::stateId(const Name &stateName) const
{
    DE_ASSERT(d->stateNames.contains(stateName));
    return d->stateNames[stateName];
}

void Drawable::addBuffer(Id id, GLBuffer *buffer)
{
    addBuffer(id, std::shared_ptr<GLBuffer>(buffer));
}

void Drawable::addBuffer(Id id, const std::shared_ptr<GLBuffer>& buffer)
{
    removeBuffer(id);

    d->buffers[id] = buffer;
    setProgram(id, d->defaultProgram);
    insert(*buffer, Required);
}

Drawable::Id Drawable::addBuffer(const Name &bufferName, GLBuffer *buffer)
{
    Id id = d->nextBufferId();
    d->bufferNames.insert(bufferName, id);
    addBuffer(id, buffer);
    return id;
}

Drawable::Id Drawable::addBuffer(GLBuffer *buffer)
{
    return addBuffer(std::shared_ptr<GLBuffer>(buffer));
}

Drawable::Id Drawable::addBuffer(const std::shared_ptr<GLBuffer>& buffer)
{
    const Id id = d->nextBufferId();
    addBuffer(id, buffer);
    return id;
}

Drawable::Id Drawable::addBufferWithNewProgram(GLBuffer *buffer, const Name &programName)
{
    // Take ownership of the buffer.
    const Id bufId = d->nextBufferId();
    addBuffer(bufId, buffer);
    // Assign a new program to the buffer.
    const Id progId = addProgram(programName);
    setProgram(bufId, program(progId));
    return bufId;
}

void Drawable::addBufferWithNewProgram(Id id, GLBuffer *buffer, const Name &programName)
{
    addBuffer(id, buffer);
    addProgram(programName);
    setProgram(id, programName);
}

Drawable::Id Drawable::addBufferWithNewProgram(const Name &bufferName, GLBuffer *buffer,
                                               const Name &programName)
{
    const Id progId = addProgram(programName);
    const Id bufId = addBuffer(bufferName, buffer);
    setProgram(bufId, program(progId));
    return bufId;
}

GLProgram &Drawable::addProgram(Id id)
{
    // Program 0 is the default program.
    DE_ASSERT(id != 0);

    removeProgram(id);

    GLProgram *p = new GLProgram;
    d->programs[id] = p;
    insert(*p, Required);
    return *p;
}

Drawable::Id Drawable::addProgram(const Name &programName)
{
    const Id id = d->nextProgramId();
    addProgram(id);
    if (!programName.isEmpty())
    {
        d->programNames.insert(programName, id);
    }
    return id;
}

GLState &Drawable::addState(Id id, const GLState &state)
{
    removeState(id);
    GLState *s = new GLState(state);
    d->states[id] = s;
    return *s;
}

Drawable::Id Drawable::addState(const Name &stateName, const GLState &state)
{
    const Id id = d->nextStateId();
    addState(id, state);
    d->stateNames.insert(stateName, id);
    return id;
}

void Drawable::removeBuffer(Id id)
{
    if (d->buffers.contains(id))
    {
        remove(*d->buffers[id]);
        d->buffers.remove(id);
    }
    d->configs.remove(id);
}

void Drawable::removeProgram(Id id)
{
    if (d->programs.contains(id))
    {
        GLProgram *prog = d->programs[id];
        d->replaceProgram(prog, &d->defaultProgram);
        remove(*prog);
        delete d->programs.take(id);
    }
}

void Drawable::removeState(Id id)
{
    if (d->programs.contains(id))
    {
        GLState *st = d->states[id];
        d->replaceState(st, nullptr);
        delete d->states.take(id);
    }
}

void Drawable::removeBuffer(const Name &bufferName)
{
    const Id id = bufferId(bufferName);
    removeBuffer(id);
    d->removeName(d->bufferNames, id);
}

void Drawable::removeProgram(const Name &programName)
{
    const Id id = programId(programName);
    removeProgram(id);
    d->removeName(d->programNames, id);
}

void Drawable::removeState(const Name &stateName)
{
    const Id id = stateId(stateName);
    removeState(id);
    d->removeName(d->stateNames, id);
}

void Drawable::setProgram(Id bufferId, GLProgram &program)
{
    d->configs[bufferId].program = &program;
}

void Drawable::setProgram(Id bufferId, Id programId)
{
    d->configs[bufferId].program = &program(programId);
}

void Drawable::setProgram(Id bufferId, const Name &programName)
{
    setProgram(bufferId, program(programName));
}

void Drawable::setProgram(const Name &bufferName, GLProgram &program)
{
    setProgram(bufferId(bufferName), program);
}

void Drawable::setProgram(const Name &bufferName, const Name &programName)
{
    setProgram(bufferId(bufferName), program(programName));
}

void Drawable::setProgram(GLProgram &program)
{
    for (const auto &id : allBuffers())
    {
        setProgram(id, program);
    }
}

void Drawable::setProgram(Id programId)
{
    setProgram(program(programId));
}

void Drawable::setProgram(const Name &programName)
{
    setProgram(program(programName));
}

void Drawable::setState(Id bufferId, GLState &state)
{
    d->configs[bufferId].state = &state;
}

void Drawable::setState(Id bufferId, const Name &stateName)
{
    setState(bufferId, state(stateName));
}

void Drawable::setState(const Name &bufferName, GLState &state)
{
    setState(bufferId(bufferName), state);
}

void Drawable::setState(const Name &bufferName, const Name &stateName)
{
    setState(bufferId(bufferName), state(stateName));
}

void Drawable::setState(GLState &state)
{
    for (const Id &id : allBuffers())
    {
        setState(id, state);
    }
}

void Drawable::setState(const Name &stateName)
{
    setState(state(stateName));
}

void Drawable::unsetState(Id bufferId)
{
    d->configs[bufferId].state = nullptr;
}

void Drawable::unsetState(const Name &bufferName)
{
    unsetState(bufferId(bufferName));
}

void Drawable::unsetState()
{
    for (const Id &id : allBuffers())
    {
        unsetState(id);
    }
}

void Drawable::draw() const
{
    // Ignore the draw request until everything is ready.
    if (!isReady()) return;

    const GLProgram *currentProgram = nullptr;
    const GLState *currentState   = nullptr;

    // Make sure the GL state on the top of the stack is in effect.
    GLState::current().apply();

    for (const auto &i : d->buffers)
    {
        const Id id = i.first;

        // Switch the program if necessary.
        const GLProgram &bufProg = programForBuffer(id);
        if (currentProgram != &bufProg)
        {
            if (currentProgram) currentProgram->endUse();

            currentProgram = &bufProg;
            currentProgram->beginUse();
        }

        // If a state has been defined, use it.
        const GLState *bufState = stateForBuffer(id);
        if (bufState && currentState != bufState)
        {
            currentState = bufState;
            currentState->apply();
        }
        else if (!bufState && currentState != nullptr)
        {
            // Use the current state from the stack.
            currentState = nullptr;
            GLState::current().apply();
        }

        // Ready to draw.
        i.second->draw();
    }

    // Cleanup.
    if (currentProgram)
    {
        currentProgram->endUse();
    }
    if (currentState)
    {
        // We messed with the state; restore to what the stack says is current.
        GLState::current().apply();
    }
}

} // namespace de
