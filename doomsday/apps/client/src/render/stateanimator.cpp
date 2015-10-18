/** @file stateanimator.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "render/stateanimator.h"
#include "render/rendersystem.h"
#include "clientapp.h"
#include "dd_loop.h"

#include <de/ScriptedInfo>
#include <de/RecordValue>
#include <de/NativeValue>
#include <de/GLUniform>

using namespace de;

static String const DEF_PROBABILITY("prob");
static String const DEF_ROOT_NODE  ("node");
static String const DEF_LOOPING    ("looping");
static String const DEF_PRIORITY   ("priority");
static String const DEF_PASS       ("pass");
static String const DEF_VARIABLE   ("variable");
static String const DEF_WRAP       ("wrap");
static String const DEF_ENABLED    ("enabled");
static String const DEF_MATERIAL   ("material");

static String const VAR_SELF    ("self");
static String const VAR_ID      ("__id__");
static String const VAR_ASSET   ("asset");
static String const VAR_ENABLED ("enabled");
static String const VAR_MATERIAL("material");

static String const PASS_GLOBAL("");
static String const DEFAULT_MATERIAL("default");

static int const ANIM_DEFAULT_PRIORITY = 1;

DENG2_PIMPL(StateAnimator)
, DENG2_OBSERVES(Variable, Change)
{
    /**
     * Specialized animation sequence state for a running animation.
     */
    struct Sequence : public ModelDrawable::Animator::OngoingSequence
    {
        enum LoopMode {
            NotLooping = 0,
            Looping = 1
        };

        LoopMode looping = NotLooping;
        int priority = ANIM_DEFAULT_PRIORITY;
        Scheduler const *timeline = nullptr; // owned by ModelRenderer::AnimSequence
        std::unique_ptr<Scheduler::Clock> clock;

        Sequence() {}

        Sequence(int animationId, String const &rootNode, LoopMode looping, int priority,
                 Scheduler const *timeline = nullptr)
            : looping(looping)
            , priority(priority)
            , timeline(timeline)
        {
            animId = animationId;
            node   = rootNode;
        }

        Sequence(Sequence const &other) : OngoingSequence(other)
        {
            apply(other);
        }

        Sequence &operator = (Sequence const &other)
        {
            apply(other);
            return *this;
        }

        void apply(Sequence const &other)
        {
            animId   = other.animId;
            node     = other.node;
            looping  = other.looping;
            priority = other.priority;
            timeline = other.timeline;

            clock.reset();
        }

        bool isRunning() const
        {
            if(looping == Looping)
            {
                // Looping animations are always running.
                return true;
            }
            return !atEnd();
        }

        static Sequence *make() { return new Sequence; }
    };

    ModelRenderer::AuxiliaryData const *auxData;
    QHash<String, Sequence> pendingAnimForNode;
    String currentStateName;
    Record names; ///< Local context for scripts.
    QBitArray passMask;
    QHash<String, int> passIndexLookup;

    /**
     * Animatable variable bound to a GL uniform. The value can have 1...4 float
     * components.
     */
    struct RenderVar
    {
        struct Value {
            Animation anim;
            Rangef wrap;

            Value(Animation const &a) : anim(a) {}
        };
        QList<Value> values;
        GLUniform *uniform = nullptr;

        void init(float value)
        {
            values.clear();
            values.append(Animation(value, Animation::Linear));
        }

        template <typename VecType>
        void init(VecType const &vec)
        {
            values.clear();
            for(int i = 0; i < vec.size(); ++i)
            {
                values.append(Animation(vec[i], Animation::Linear));
            }
        }

        ~RenderVar()
        {
            delete uniform;
        }

        float currentValue(int index) const
        {
            auto const &val = values.at(index);
            float v = val.anim.value();
            if(val.wrap.isEmpty())
            {
                return v;
            }
            return val.wrap.wrap(v);
        }

        /**
         * Copies the current values to the uniform.
         */
        void updateUniform()
        {
            switch(values.size())
            {
            case 1:
                *uniform = currentValue(0);
                break;

            case 2:
                *uniform = Vector2f(currentValue(0),
                                    currentValue(1));
                break;

            case 3:
                *uniform = Vector3f(currentValue(0),
                                    currentValue(1),
                                    currentValue(2));
                break;

            case 4:
                *uniform = Vector4f(currentValue(0),
                                    currentValue(1),
                                    currentValue(2),
                                    currentValue(3));
                break;
            }
        }
    };

    typedef QHash<String, RenderVar *> RenderVars;
    QHash<String, RenderVars> passVars;

    QHash<String, Variable *> passMaterials;

    Instance(Public *i, DotPath const &id)
        : Base(i)
        , auxData(ClientApp::renderSystem().modelRenderer().auxiliaryData(id))
    {
        names.addText(VAR_ID, id).setReadOnly();
        names.add(VAR_ASSET).set(new RecordValue(App::asset(id).accessedRecord())).setReadOnly();

        // VAR_SELF should point to the thing's namespace, or player's for psprites

        initShaderVariables();
    }

    ~Instance()
    {
        deinitShaderVariables();
    }

    void initShaderVariables()
    {
        int passIndex = 0;
        passIndexLookup.clear();

        auto const &def = names[VAR_ASSET].valueAsRecord();
        if(def.has("render"))
        {
            Record const &renderBlock = def.subrecord("render");

            initVariablesForPass(renderBlock);

            // Each rendering pass is represented by a subrecord, named
            // according the to the pass names.
            auto passes = ScriptedInfo::subrecordsOfType(DEF_PASS, renderBlock);
            DENG2_FOR_EACH_CONST(Record::Subrecords, i, passes)
            {
                passIndexLookup[i.key()] = passIndex++;

                Record &passRec = names.addRecord(i.key());
                passRec.addBoolean(VAR_ENABLED,
                                   ScriptedInfo::isTrue(*i.value(), DEF_ENABLED, true))
                       .audienceForChange() += this;

                initVariablesForPass(*i.value(), i.key());
            }
        }

        DENG2_ASSERT(passIndex == auxData->passes.size());
        DENG2_ASSERT(passIndexLookup.size() == auxData->passes.size());

        passMask.resize(auxData->passes.size());
        updatePassMask();

        qDebug() << "Namespaces:\n" << names.asText();
    }

    void initVariablesForPass(Record const &block, String const &passName = PASS_GLOBAL)
    {
        static char const *componentNames[] = { "x", "y", "z", "w" };

        // Each pass has a variable for selecting the material.
        // The default value is optionally specified in the definition.
        passMaterials.insert(passName,
                             &names.addText(passName.concatenateMember(VAR_MATERIAL),
                                            block.gets(DEF_MATERIAL, DEFAULT_MATERIAL)));

        /// @todo Should observe if the variable above is deleted unexpectedly. -jk

        // Look up the variable declarations.
        auto vars = ScriptedInfo::subrecordsOfType(DEF_VARIABLE, block);
        DENG2_FOR_EACH_CONST(Record::Subrecords, i, vars)
        {
            std::unique_ptr<RenderVar> var(new RenderVar);

            GLUniform::Type uniformType = GLUniform::Float;

            // Initialize the appropriate type of value animation and uniform,
            // depending on the "value" key in the definition.
            auto const &valueDef = *i.value();
            Value const &initialValue = valueDef["value"].value();
            if(auto const *array = initialValue.maybeAs<ArrayValue>())
            {
                switch(array->size())
                {
                default:
                    throw DefinitionError("StateAnimator::initVariables",
                                          QString("%1: Invalid initial value size (%2) for render.variable")
                                          .arg(valueDef.gets(ScriptedInfo::VAR_SOURCE))
                                          .arg(array->size()));

                case 2:
                    var->init(vectorFromValue<Vector2f>(*array));
                    uniformType = GLUniform::Vec2;
                    break;

                case 3:
                    var->init(vectorFromValue<Vector3f>(*array));
                    uniformType = GLUniform::Vec3;
                    break;

                case 4:
                    var->init(vectorFromValue<Vector4f>(*array));
                    uniformType = GLUniform::Vec4;
                    break;
                }

                // Expose the components individually in the namespace for scripts.
                for(int k = 0; k < var->values.size(); ++k)
                {
                    addBinding(passName.concatenateMember(String(i.key()).concatenateMember(componentNames[k])),
                               var->values[k].anim);
                }
            }
            else
            {
                var->init(float(initialValue.asNumber()));

                // Expose in the namespace for scripts.
                addBinding(passName.concatenateMember(i.key()),
                           var->values[0].anim);
            }

            // Optional range wrapping.
            if(valueDef.hasSubrecord(DEF_WRAP))
            {
                for(int k = 0; k < 4; ++k)
                {
                    String const varName = QString("%1.%2").arg(DEF_WRAP).arg(componentNames[k]);
                    if(valueDef.has(varName))
                    {
                        var->values[k].wrap = rangeFromValue<Rangef>(valueDef.geta(varName));
                    }
                }
            }
            else if(valueDef.has(DEF_WRAP))
            {
                var->values[0].wrap = rangeFromValue<Rangef>(valueDef.geta(DEF_WRAP));
            }

            // Uniform to be passed to the shader.
            var->uniform = new GLUniform(i.key().toLatin1(), uniformType);

            passVars[passName][i.key()] = var.release();
        }
    }

    void addBinding(String const &varName, Animation &anim)
    {
        names.add(varName)
                .set(new NativeValue(&anim, &ScriptSystem::builtInClass("Animation")))
                .setReadOnly();
    }

    void deinitShaderVariables()
    {
        for(RenderVars const &vars : passVars.values())
        {
            qDeleteAll(vars.values());
        }
        passVars.clear();
        passMaterials.clear();
    }

    void variableValueChanged(Variable &, Value const &)
    {
        // This is called when one of the "(pass).enabled" variables is modified.
        updatePassMask();
    }

    void updatePassMask()
    {
        Record::Subrecords enabledPasses = names.subrecords([] (Record const &sub) {
            return sub.getb(DEF_ENABLED, false);
        });

        passMask.fill(false);
        for(String name : enabledPasses.keys())
        {
            DENG2_ASSERT(passIndexLookup.contains(name));
            passMask.setBit(passIndexLookup[name], true);
        }
    }

    int animationId(String const &name) const
    {
        return ModelRenderer::identifierFromText(name, [this] (String const &name) {
            return self.model().animationIdForName(name);
        });
    }

    Sequence &start(Sequence const &spec)
    {
        Sequence &anim = self.start(spec.animId, spec.node).as<Sequence>();
        anim.apply(spec);
        if(anim.timeline)
        {
            anim.clock.reset(new Scheduler::Clock(*anim.timeline, &names));
        }
        applyFlagOperation(anim.flags, Sequence::ClampToDuration,
                           anim.looping == Sequence::Looping? UnsetFlags : SetFlags);
        return anim;
    }
};

StateAnimator::StateAnimator(DotPath const &id, ModelDrawable const &model)
    : ModelDrawable::Animator(model, Instance::Sequence::make)
    , d(new Instance(this, id))
{}

void StateAnimator::setOwnerNamespace(Record &names)
{
    d->names.add(VAR_SELF).set(new RecordValue(names));
}

void StateAnimator::triggerByState(String const &stateName)
{
    using Sequence = Instance::Sequence;

    // No animations can be triggered if none are available.
    auto const *stateAnims = (d->auxData? &d->auxData->animations : nullptr);
    if(!stateAnims) return;

    auto found = stateAnims->constFind(stateName);
    if(found == stateAnims->constEnd()) return;

    LOG_AS("StateAnimator");
    LOGDEV_GL_XVERBOSE("triggerByState: ") << stateName;

    d->currentStateName = stateName;

    foreach(ModelRenderer::AnimSequence const &seq, found.value())
    {
        try
        {
            // Test for the probability of this animation.
            float chance = seq.def->getf(DEF_PROBABILITY, 1.f);
            if(frand() > chance) continue;

            // Start the animation on the specified node (defaults to root),
            // unless it is already running.
            String const node = seq.def->gets(DEF_ROOT_NODE, "");
            int animId = d->animationId(seq.name);

            // Do not restart running sequences.
            /// @todo Only restart the animation if the current state is not the expected
            /// one (checking the state cycle).
            if(isRunning(animId, node)) continue;

            int const priority = seq.def->geti(DEF_PRIORITY, ANIM_DEFAULT_PRIORITY);

            // Loop up the timeline.
            Scheduler *timeline = seq.timeline;
            if(!seq.sharedTimeline.isEmpty())
            {
                auto tl = d->auxData->timelines.constFind(seq.sharedTimeline);
                if(tl != d->auxData->timelines.constEnd())
                {
                    timeline = tl.value();
                }
            }

            // Parameters for the new sequence.
            Sequence anim(animId, node,
                          ScriptedInfo::isTrue(*seq.def, DEF_LOOPING)? Sequence::Looping :
                                                                       Sequence::NotLooping,
                          priority,
                          timeline);

            // Do not override higher-priority animations.
            if(auto *existing = find(node)->maybeAs<Sequence>())
            {
                if(priority < existing->priority)
                {
                    // This will be started once the higher-priority animation
                    // has finished.
                    d->pendingAnimForNode[node] = anim;
                    continue;
                }
            }

            d->pendingAnimForNode.remove(node);

            // Start a new sequence.
            d->start(anim);
        }
        catch(ModelDrawable::Animator::InvalidError const &er)
        {
            LOGDEV_GL_WARNING("Failed to start animation \"%s\": %s")
                    << seq.name << er.asText();
            continue;
        }

        LOG_GL_VERBOSE("Starting animation: " _E(b)) << seq.name;
        break;
    }
}

void StateAnimator::advanceTime(TimeDelta const &elapsed)
{
    ModelDrawable::Animator::advanceTime(elapsed);

    using Sequence = Instance::Sequence;
    bool retrigger = false;

    for(int i = 0; i < count(); ++i)
    {
        auto &anim = at(i).as<Sequence>();
        ddouble factor = 1.0;
        // TODO: Determine actual time factor.

        // Advance the sequence.
        TimeDelta animElapsed = factor * elapsed;
        anim.time += animElapsed;

        if(anim.looping == Sequence::NotLooping)
        {
            // Clamp at the end.
            anim.time = min(anim.time, anim.duration);
        }

        if(anim.looping == Sequence::Looping)
        {
            // When a looping animation has completed a loop, it may still
            // trigger a variant.
            if(anim.atEnd())
            {
                retrigger = true;
                anim.time -= anim.duration; // Trigger only once per loop.
                if(anim.clock)
                {
                    anim.clock->rewind(anim.time);
                }
            }
        }

        // Scheduled events.
        if(anim.clock)
        {
            anim.clock->advanceTime(animElapsed);
        }

        // Stop finished animations.
        if(!anim.isRunning())
        {
            String const node = anim.node;

            // Keep the last animation intact so there's something to update the
            // model state with (ModelDrawable being shared with multiple objects,
            // so each animator needs to retain its bone transformations).
            if(count() > 1)
            {
                stop(i--); // `anim` gets deleted
            }

            // Start a previously triggered pending animation.
            auto &pending = d->pendingAnimForNode;
            if(pending.contains(node))
            {
                LOG_GL_VERBOSE("Starting pending animation %i") << pending[node].animId;
                d->start(pending[node]);
                pending.remove(node);
            }
        }
    }

    if(retrigger && !d->currentStateName.isEmpty())
    {
        triggerByState(d->currentStateName);
    }
}

ddouble StateAnimator::currentTime(int index) const
{
    // Mobjs think on sharp ticks only, however we need to ensure time advances on
    // every frame for smooth animation.
    return ModelDrawable::Animator::currentTime(index); // + frameTimePos;
}

QBitArray StateAnimator::passMask() const
{
    return d->passMask;
}

duint StateAnimator::materialForPass(String const &passName) const
{
   auto const iter = d->passMaterials.constFind(passName);
   if(iter != d->passMaterials.constEnd())
   {
       Variable const *material = iter.value();
       DENG2_ASSERT(material != nullptr);
       auto const matIndex = d->auxData->materialIndexForName.constFind(material->value().asText());
       if(matIndex != d->auxData->materialIndexForName.constEnd())
       {
           return matIndex.value();
       }
   }
   else
   {
       return materialForPass(PASS_GLOBAL);
   }
   return 0; // default material
}

void StateAnimator::bindUniforms(GLProgram &program, BindOperation operation) const
{
    bindPassUniforms(program, PASS_GLOBAL, operation);
}

void StateAnimator::bindPassUniforms(GLProgram &program, String const &passName,
                                    BindOperation operation) const
{
    auto const vars = d->passVars.constFind(passName);
    if(vars != d->passVars.constEnd())
    {
        for(auto i : vars.value())
        {
            if(operation == Bind)
            {
                i->updateUniform();
                program.bind(*i->uniform);
            }
            else
            {
                program.unbind(*i->uniform);
            }
        }
    }
}
