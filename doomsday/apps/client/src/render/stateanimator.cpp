/** @file stateanimator.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "render/modelloader.h"
#include "render/shadervar.h"
#include "clientapp.h"
#include "dd_loop.h"

#include <doomsday/world/thinkerdata.h>

#include <de/ConditionalTrigger>
#include <de/ScriptedInfo>
#include <de/ScriptSystem>
#include <de/RecordValue>
#include <de/NoneValue>
#include <de/NativePointerValue>
#include <de/GLUniform>

#include <QSet>

using namespace de;

namespace render {

static String const DEF_ALWAYS_TRIGGER("alwaysTrigger");
static String const DEF_MUST_FINISH   ("mustFinish");
static String const DEF_ANGLE         ("angle");
static String const DEF_AXIS          ("axis");
static String const DEF_DURATION      ("duration");
static String const DEF_ENABLED       ("enabled");
static String const DEF_LOOPING       ("looping");
static String const DEF_NODE          ("node");
static String const DEF_PRIORITY      ("priority");
static String const DEF_PROBABILITY   ("prob");
static String const DEF_SPEED         ("speed");
static String const DEF_VARIABLE      ("variable");

static String const VAR_ID            ("ID");           // model asset ID
static String const VAR_ASSET         ("__asset__");    // runtime reference to asset metadata
static String const VAR_ENABLED       ("enabled");
static String const VAR_MATERIAL      ("material");

static String const VAR_NOTIFIED_STATES("notifiedStates");

static String const PASS_GLOBAL       ("");
static String const DEFAULT_MATERIAL  ("default");

static int const ANIM_DEFAULT_PRIORITY = 1;

DE_PIMPL(StateAnimator)
, DE_OBSERVES(Variable, Change)
{
    enum BindOperation { Bind, Unbind };

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
        bool mustFinish = false; // Sequence will play to end unless higher priority started.
        int priority = ANIM_DEFAULT_PRIORITY;
        Timeline const *timeline = nullptr; // owned by ModelRenderer::AnimSequence
        std::unique_ptr<Timeline::Clock> clock;
        TimeSpan overrideDuration = -1.0;
        TimeSpan actualRuntime = 0.0;

        Sequence() {}

        Sequence(int animationId, String const &rootNode, LoopMode looping, bool mustFinish,
                 int priority, Timeline const *timeline = nullptr)
            : looping(looping)
            , mustFinish(mustFinish)
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

        Sequence &operator=(Sequence const &other)
        {
            apply(other);
            return *this;
        }

        void initialize() override
        {
            ModelDrawable::Animator::OngoingSequence::initialize();
        }

        void apply(Sequence const &other)
        {
            animId     = other.animId;
            node       = other.node;
            looping    = other.looping;
            mustFinish = other.mustFinish;
            priority   = other.priority;
            timeline   = other.timeline;

            if (other.overrideDuration >= 0)
            {
                duration = other.overrideDuration;
            }

            clock.reset();
        }

        bool isRunning() const
        {
            if (looping == Looping)
            {
                // Looping animations are always running.
                return true;
            }
            return !atEnd();
        }

        int loopCount() const
        {
            if (duration > 0)
            {
                return int(actualRuntime / duration);
            }
            return 0;
        }

        void operator>>(Writer &to) const override
        {
            ModelDrawable::Animator::OngoingSequence::operator>>(to);
            to << duint8(looping) << priority;
        }

        void operator<<(Reader &from) override
        {
            ModelDrawable::Animator::OngoingSequence::operator<<(from);
            from.readAs<duint8>(looping);
            from >> priority;
        }

        static Sequence *make() { return new Sequence; }
    };

    QHash<String, Sequence> pendingAnimForNode;
    String currentStateName;
    Record names; ///< Local context for scripts, i.e., per-object model state.
    String ownerNamespaceVarName;

    ModelDrawable::Appearance appearance;

    // Lookups used when drawing or updating state:
    QHash<String, int> indexForPassName;
    QHash<Variable *, int> passForMaterialVariable;
    QHash<String, ShaderVars *> passVars;

    struct AnimVar
    {
        String variableName;
        SafePtr<AnimationValue> angle; // not owned
        /// Units per second; added to value independently of its animation.
        SafePtr<AnimationValue> speed; // not owned
        Vec3f axis;
    };
    typedef QHash<String, AnimVar *> AnimVars;
    AnimVars animVars;

    /// Optional script callback for chosen states.
    struct StateCallback : public ConditionalTrigger
    {
        Record &names;

        StateCallback(Record &names) : names(names)
        {
            setCondition(names[VAR_NOTIFIED_STATES]);
        }

        void handleTriggered(String const &trigger) override
        {
            Record ns;
            ns.add(QStringLiteral("self")).set(new RecordValue(names));
            Process::scriptCall(Process::IgnoreResult, ns,
                                QStringLiteral("self.__asset__.onStateChange"),
                                "$self",    // StateAnimator instance
                                trigger);   // new state
        }
    };
    std::unique_ptr<StateCallback> stateCallback;
    std::unique_ptr<Scheduler> scheduler; // only created if needed for additional timelines

    Impl(Public *i, DotPath const &assetId) : Base(i)
    {
        if (!assetId.isEmpty())
        {
            initVariables(assetId);

            // Set up the model drawing parameters.
            if (!self().model().passes.isEmpty())
            {
                appearance.drawPasses = &self().model().passes;
            }
            appearance.programCallback = [this] (GLProgram &program, ModelDrawable::ProgramBinding binding)
            {
                bindUniforms(program,
                    binding == ModelDrawable::AboutToBind? Bind : Unbind);
            };
            appearance.passCallback = [this] (ModelDrawable::Pass const &pass, ModelDrawable::PassState state)
            {
                bindPassUniforms(*self().model().currentProgram(),
                    pass.name,
                    state == ModelDrawable::PassBegun? Bind : Unbind);
            };
        }
    }

    ~Impl()
    {
        deinitVariables();
    }

    void initVariables(DotPath const &assetId)
    {
        // Initialize the StateAnimator script object.
        names.add(Record::VAR_NATIVE_SELF).set(new NativePointerValue(thisPublic)).setReadOnly();
        names.addSuperRecord(ScriptSystem::builtInClass(QStringLiteral("Render"),
                                                        QStringLiteral("StateAnimator")));
        names.addText(VAR_ID, assetId).setReadOnly();
        Record const &assetDef = App::asset(assetId).accessedRecord();
        names.add(VAR_ASSET).set(new RecordValue(assetDef)).setReadOnly();
        if (assetDef.has(VAR_NOTIFIED_STATES))
        {
            // Use the initial value for state callbacks.
            names.add(VAR_NOTIFIED_STATES).set(assetDef.get(VAR_NOTIFIED_STATES));
        }
        else
        {
            // The states to notify can be chosen later.
            names.addArray(VAR_NOTIFIED_STATES);
        }
        if (assetDef.has(QStringLiteral("onStateChange")))
        {
            stateCallback.reset(new StateCallback(names));
        }

        int const passCount = self().model().passes.size();

        // Clear lookups affected by the variables.
        indexForPassName.clear();
        appearance.passMaterial.clear();
        if (!passCount)
        {
            // Material to be used with the default pass.
            appearance.passMaterial << 0;
        }
        else
        {
            for (int i = 0; i < passCount; ++i)
            {
                appearance.passMaterial << 0;
            }
        }
        appearance.passMask.resize(passCount);

        // The main material variable should always exist. The "render" definition
        // may override this default value.
        names.addText(VAR_MATERIAL, DEFAULT_MATERIAL);

        int passIndex = 0;
        auto const &def = names[VAR_ASSET].valueAsRecord();
        if (def.has(ModelLoader::DEF_RENDER))
        {
            Record const &renderBlock = def.subrecord(ModelLoader::DEF_RENDER);

            initVariablesForPass(renderBlock);

            // Each rendering pass is represented by a subrecord, named
            // according the to the pass names.
            auto passes = ScriptedInfo::subrecordsOfType(ModelLoader::DEF_PASS, renderBlock);
            for (String passName : ScriptedInfo::sortRecordsBySource(passes))
            {
                Record const &passDef = *passes[passName];

                indexForPassName[passName] = passIndex++;

                Record &passRec = names.addSubrecord(passName);
                passRec.addBoolean(VAR_ENABLED,
                                   ScriptedInfo::isTrue(passDef, DEF_ENABLED, true))
                       .audienceForChange() += this;

                initVariablesForPass(passDef, passName);
            }
        }

        if (def.has(ModelLoader::DEF_ANIMATION))
        {
            auto varDefs = ScriptedInfo::subrecordsOfType(DEF_VARIABLE, def.subrecord(ModelLoader::DEF_ANIMATION));
            for (String varName : varDefs.keys())
            {
                initAnimationVariable(varName, *varDefs[varName]);
            }
        }

        DE_ASSERT(passIndex == passCount);
        DE_ASSERT(indexForPassName.size() == passCount);

        updatePassMask();
        updatePassMaterials();
    }

    void initVariablesForPass(Record const &block,
                              String const &passName = PASS_GLOBAL)
    {
        // Each pass has a variable for selecting the material.
        // The default value is optionally specified in the definition.
        Variable &passMaterialVar = names.addText(passName.concatenateMember(VAR_MATERIAL),
                                                  block.gets(ModelLoader::DEF_MATERIAL, DEFAULT_MATERIAL));
        passMaterialVar.audienceForChange() += this;
        passForMaterialVariable.insert(&passMaterialVar, self().model().passes.findName(passName));

        /// @todo Should observe if the variable above is deleted unexpectedly. -jk

        ShaderVars *vars = passVars[passName];
        if (!vars)
        {
            vars = new ShaderVars;
            passVars.insert(passName, vars);
        }

        // Create the animated variables to be used with the shader based
        // on the pass definitions.
        auto varDefs = ScriptedInfo::subrecordsOfType(DEF_VARIABLE, block);
        for (auto i = varDefs.constBegin(); i != varDefs.constEnd(); ++i)
        {
            vars->initVariableFromDefinition(
                        i.key(), *i.value(),
                        names.addSubrecord(passName, Record::KeepExisting));
        }
    }

    void initAnimationVariable(String const &variableName,
                               Record const &variableDef)
    {
        try
        {
            std::unique_ptr<AnimVar> var(new AnimVar);
            var->variableName = variableName;
            var->angle.reset(new AnimationValue(Animation(variableDef.getf(DEF_ANGLE, 0.f), Animation::Linear)));
            var->speed.reset(new AnimationValue(Animation(variableDef.getf(DEF_SPEED, 0.f), Animation::Linear)));
            var->axis = vectorFromValue<Vec3f>(variableDef.get(DEF_AXIS));

            addBinding(variableName.concatenateMember(DEF_ANGLE), var->angle);
            addBinding(variableName.concatenateMember(DEF_SPEED), var->speed);

            animVars.insert(variableDef.gets(DEF_NODE), var.get());
            var.release();

            // The model should now be transformed even without active
            // animation sequences so that the variables are applied.
            self().setFlags(AlwaysTransformNodes);
        }
        catch (Error const &er)
        {
            LOG_GL_WARNING("%s: %s") << ScriptedInfo::sourceLocation(variableDef)
                                     << er.asText();
        }
    }

    void addBinding(String const &varName, AnimationValue *anim)
    {
        names.add(varName).set(anim).setReadOnly(); // ownership of anim taken
                //.set(new NativePointerValue(&anim, &ScriptSystem::builtInClass("Animation")))
                //.setReadOnly();
    }

    void deinitVariables()
    {
        appearance.passMaterial.clear();

        // Shader variables.
        qDeleteAll(passVars.values());
        passVars.clear();

        // Animator variables.
        qDeleteAll(animVars.values());
        animVars.clear();
    }

    Variable const &materialVariableForPass(duint passIndex) const
    {
        auto const &model = self().model();
        if (!model.passes.isEmpty())
        {
            String const varName = model.passes.at(passIndex).name.concatenateMember(VAR_MATERIAL);
            if (names.has(varName))
            {
                return names[varName];
            }
        }
        return names[VAR_MATERIAL];
    }

    void updatePassMaterials()
    {
        for (int i = 0; i < appearance.passMaterial.size(); ++i)
        {
            appearance.passMaterial[i] = materialForUserProvidedName(
                        materialVariableForPass(i).value().asText());
        }
    }

    void variableValueChanged(Variable &var, Value const &newValue)
    {
        if (var.name() == VAR_MATERIAL)
        {
            // Update the corresponding pass material.
            int passIndex = passForMaterialVariable[&var];
            if (passIndex < 0)
            {
                updatePassMaterials();
            }
            else
            {
                appearance.passMaterial[passIndex] = materialForUserProvidedName(newValue.asText());
            }
        }
        else
        {
            DE_ASSERT(var.name() == VAR_ENABLED);

            // This is called when one of the "(pass).enabled" variables is modified.
            updatePassMask();
        }
    }

    /**
     * Determines the material to use during a rendering pass. These are
     * determined by the "material" variables in the object's namespace.
     *
     * @param materialName  Name of the material to use.
     *
     * @return Material index.
     */
    duint materialForUserProvidedName(String const &materialName) const
    {
       auto const &model = self().model();
       auto const matIndex = model.materialIndexForName.constFind(materialName);
       if (matIndex != model.materialIndexForName.constEnd())
       {
           return matIndex.value();
       }
       LOG_RES_WARNING("Asset \"%s\" does not have a material called '%s'")
               << names.gets(VAR_ID) << materialName;
       return 0; // default material
    }

    void updatePassMask()
    {
        Record::Subrecords enabledPasses = names.subrecords([] (Record const &sub) {
            return sub.getb(DEF_ENABLED, false);
        });

        appearance.passMask.fill(false);
        for (String name : enabledPasses.keys())
        {
            DE_ASSERT(indexForPassName.contains(name));
            appearance.passMask.setBit(indexForPassName[name], true);
        }
    }

    void updateAnimationValuePointers()
    {
        foreach (String passName, passVars.keys())
        {
            ShaderVars *vars = passVars[passName];
            foreach (String name, vars->members.keys())
            {
                vars->members[name]->updateValuePointers(names, passName.concatenateMember(name));
            }
        }

        foreach (String name, animVars.keys())
        {
            AnimVar &animVar = *animVars[name];
            animVar.angle = &names[animVar.variableName.concatenateMember(DEF_ANGLE)].value<AnimationValue>();
            animVar.speed = &names[animVar.variableName.concatenateMember(DEF_SPEED)].value<AnimationValue>();
        }
    }

    /**
     * Checks if a shader definition has a declaration for a variable.
     *
     * @param program  Shader definition.
     * @param uniform  Uniform.
     * @return @c true, if a variable exists matching @a uniform.
     */
    static bool hasDeclaredVariable(Record const &shaderDef, GLUniform const &uniform)
    {
        return shaderDef.hasMember(String::fromUtf8(uniform.name()));
    }

    /**
     * Binds or unbinds uniforms that apply to all rendering passes.
     *
     * @param program    Program where bindings are made.
     * @param operation  Bind or unbind.
     */
    void bindUniforms(de::GLProgram &program, BindOperation operation) const
    {
        bindPassUniforms(program, PASS_GLOBAL, operation);
    }

    /**
     * Binds or unbinds uniforms that apply to a single rendering pass.
     *
     * @param program    Program where bindings are made.
     * @param passName   Name of the rendering pass. The render variables are
     *                   named, e.g., "render.(passName).uName".
     * @param operation  Bind or unbind.
     */
    void bindPassUniforms(de::GLProgram &program,
                          de::String const &passName,
                          BindOperation operation) const
    {
        auto const &modelLoader = ClientApp::renderSystem().modelRenderer().loader();

        auto const vars = passVars.constFind(passName);
        if (vars != passVars.constEnd())
        {
            for (auto i : vars.value()->members)
            {
                if (!hasDeclaredVariable(modelLoader.shaderDefinition(program),
                                         *i->uniform))
                    continue;

                if (operation == Bind)
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

    int animationId(String const &name) const
    {
        return ModelLoader::identifierFromText(name, [this] (String const &name) {
            return self().model().animationIdForName(name);
        });
    }

    Sequence &start(Sequence const &spec)
    {
        //qDebug() << "[StateAnimator] start id" << spec.animId;

        Sequence &anim = self().start(spec.animId, spec.node).as<Sequence>();
        anim.apply(spec);
        if (anim.timeline)
        {
            anim.clock.reset(new Timeline::Clock(*anim.timeline, &names));
        }
        applyFlagOperation(anim.flags, Sequence::ClampToDuration,
                           anim.looping == Sequence::Looping? UnsetFlags : SetFlags);
        return anim;
    }
};

StateAnimator::StateAnimator()
    : ModelDrawable::Animator(Impl::Sequence::make)
    , d(new Impl(this, {}))
{}

StateAnimator::StateAnimator(DotPath const &id, Model const &model)
    : ModelDrawable::Animator(model, Impl::Sequence::make)
    , d(new Impl(this, id))
{}

Model const &StateAnimator::model() const
{
    return static_cast<Model const &>(ModelDrawable::Animator::model());
}

Scheduler &StateAnimator::scheduler()
{
    if (!d->scheduler)
    {
        d->scheduler.reset(new Scheduler);
    }
    return *d->scheduler;
}

void StateAnimator::setOwnerNamespace(Record &names, String const &varName)
{
    d->ownerNamespaceVarName = varName;
    d->names.add(d->ownerNamespaceVarName).set(new RecordValue(names));

    // Call the onInit() function if there is one.
    if (d->names.has(QStringLiteral("__asset__.onInit")))
    {
        Record ns;
        ns.add(QStringLiteral("self")).set(new RecordValue(d->names));
        Process::scriptCall(Process::IgnoreResult, ns,
                            "self.__asset__.onInit",
                            "$self");
    }
}

String StateAnimator::ownerNamespaceName() const
{
    return d->ownerNamespaceVarName;
}

void StateAnimator::triggerByState(String const &stateName)
{
    using Sequence = Impl::Sequence;

    if (d->stateCallback)
    {
        d->stateCallback->tryTrigger(stateName);
    }

    // No animations can be triggered if none are available.
    auto const *stateAnims = &model().animations;
    if (!stateAnims) return;

    auto found = stateAnims->constFind(stateName);
    if (found == stateAnims->constEnd()) return;

    LOG_AS("StateAnimator");
    //LOGDEV_GL_XVERBOSE("triggerByState: ") << stateName;

    d->currentStateName = stateName;

    foreach (Model::AnimSequence const &seq, found.value())
    {
        try
        {
            // Test for the probability of this animation.
            float chance = seq.def->getf(DEF_PROBABILITY, 1.f);
            if (randf() > chance) continue;

            // Start the animation on the specified node (defaults to root),
            // unless it is already running.
            String const node = seq.def->gets(DEF_NODE, "");
            int animId = d->animationId(seq.name);

            if (animId < 0)
            {
                LOG_GL_ERROR("%s: animation sequence \"%s\" not found")
                    << ScriptedInfo::sourceLocation(*seq.def) << seq.name;
                break;
            }

            const bool alwaysTrigger = ScriptedInfo::isTrue(*seq.def, DEF_ALWAYS_TRIGGER, false);
            const bool mustFinish    = ScriptedInfo::isTrue(*seq.def, DEF_MUST_FINISH, false);

            if (!alwaysTrigger)
            {
                // Do not restart running sequences.
                /// @todo Only restart the animation if the current state is not the expected
                /// one (checking the state cycle).
                if (isRunning(animId, node)) continue;
            }

            int const priority = seq.def->geti(DEF_PRIORITY, ANIM_DEFAULT_PRIORITY);

            // Look up the timeline.
            Timeline *timeline = seq.timeline;
            if (!seq.sharedTimeline.isEmpty())
            {
                auto tl = model().timelines.constFind(seq.sharedTimeline);
                if (tl != model().timelines.constEnd())
                {
                    timeline = tl.value();
                }
            }

            // Parameters for the new sequence.
            Sequence anim(animId, node,
                          ScriptedInfo::isTrue(*seq.def, DEF_LOOPING)? Sequence::Looping :
                                                                       Sequence::NotLooping,
                          mustFinish,
                          priority,
                          timeline);

            if (seq.def->has(DEF_DURATION))
            {
                // By default, the animation duration comes from the model file.
                anim.overrideDuration = seq.def->getd(DEF_DURATION);
            }

            // Do not override higher-priority animations.
            if (auto *existing = maybeAs<Sequence>(find(node)))
            {
                if (priority < existing->priority ||
                    (priority == existing->priority && existing->mustFinish &&
                     !existing->atEnd() && existing->loopCount() == 0))
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
        catch (ModelDrawable::Animator::InvalidError const &er)
        {
            LOG_GL_WARNING("Failed to start animation \"%s\": %s")
                    << seq.name << er.asText();
            continue;
        }
        break;
    }
}

void StateAnimator::triggerDamage(int points, struct mobj_s const *inflictor)
{
    /*
     * Here we check for the onDamage() function in the asset. The __asset__
     * variable holds a direct pointer to the asset definition, where the
     * function is defined.
     */
    if (d->names.has(QStringLiteral("__asset__.onDamage")))
    {
        /*
         * We need to provide the StateAnimator instance to the script as an
         * argument, because onDamage() is a member of the asset -- when the
         * method is executed, "self" refers to the asset.
         */
        Record ns;
        ns.add(QStringLiteral("self")).set(new RecordValue(d->names));
        Process::scriptCall(Process::IgnoreResult, ns,
                            QStringLiteral("self.__asset__.onDamage"),
                            "$self", points,
                            inflictor? &THINKER_DATA(inflictor->thinker, ThinkerData) :
                                       nullptr);
    }
}

void StateAnimator::startAnimation(int animationId, int priority, bool looping, String const &node)
{
    LOG_AS("StateAnimator::startAnimation");
    try
    {
        using Seq = Impl::Sequence;
        d->start(Seq(animationId,
                     node,
                     looping ? Seq::Looping : Seq::NotLooping,
                     false, // same priority override allowed
                     priority));
    }
    catch (const Error &er)
    {
        LOG_GL_ERROR("%s: %s") << d->names.gets(VAR_ID) << er.asText();
    }
}

int StateAnimator::animationId(String const &name) const
{
    return d->animationId(name);
}

void StateAnimator::advanceTime(TimeSpan const &elapsed)
{
    ModelDrawable::Animator::advanceTime(elapsed);

    using Sequence = Impl::Sequence;
    bool retrigger = false;

    // Update animation variables values.
    for (auto *var : d->animVars.values())
    {
        if (!var->angle || !var->speed) continue;

        var->angle->animation().shift(var->speed->animation() * elapsed);

        // Keep the angle in the 0..360 range.
        float varAngle = var->angle->animation();
        if (varAngle > 360)    var->angle->animation().shift(-360);
        else if (varAngle < 0) var->angle->animation().shift(+360);
    }

    for (int i = 0; i < count(); ++i)
    {
        auto &anim = at(i).as<Sequence>();
        ddouble factor = 1.0;
        // TODO: Determine actual time factor.

        // Advance the sequence.
        TimeSpan animElapsed = factor * elapsed;
        anim.time += animElapsed;
        anim.actualRuntime += animElapsed;

        if (anim.looping == Sequence::NotLooping)
        {
            // Clamp at the end.
            anim.time = min(anim.time, anim.duration);
        }

        if (anim.looping == Sequence::Looping)
        {
            // When a looping animation has completed a loop, it may still
            // trigger a variant.
            if (anim.atEnd())
            {
                retrigger = true;
                anim.time -= anim.duration; // Trigger only once per loop.
                if (anim.clock)
                {
                    anim.clock->rewind(anim.time);
                }
            }
        }

        // Scheduled events.
        if (anim.clock)
        {
            anim.clock->advanceTime(animElapsed);
        }

        // Stop finished animations.
        if (!anim.isRunning())
        {
            String const node = anim.node;

            // Keep the last animation intact so there's something to update the
            // model state with (ModelDrawable being shared with multiple objects,
            // so each animator needs to retain its bone transformations).
            if (count() > 1)
            {
                stop(i--); // `anim` gets deleted
            }

            // Start a previously triggered pending animation.
            auto &pending = d->pendingAnimForNode;
            if (pending.contains(node))
            {
                LOG_GL_VERBOSE("Starting pending animation %i") << pending[node].animId;
                d->start(pending[node]);
                pending.remove(node);
            }
        }
    }

    if (d->scheduler)
    {
        d->scheduler->advanceTime(elapsed);
    }

    if (retrigger && !d->currentStateName.isEmpty())
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

Vec4f StateAnimator::extraRotationForNode(String const &nodeName) const
{
    auto found = d->animVars.constFind(nodeName);
    if (found != d->animVars.constEnd())
    {
        Impl::AnimVar const &var = *found.value();
        if (var.angle)
        {
            return Vec4f(var.axis, var.angle->animation());
        }
    }
    return Vec4f();
}

ModelDrawable::Appearance const &StateAnimator::appearance() const
{
    return d->appearance;
}

Record &StateAnimator::objectNamespace()
{
    return d->names;
}

Record const &StateAnimator::objectNamespace() const
{
    return d->names;
}

void StateAnimator::operator >> (Writer &to) const
{
    ModelDrawable::Animator::operator >> (to);

    to << d->currentStateName
       << Record(d->names, Record::IgnoreDoubleUnderscoreMembers);
}

void StateAnimator::operator << (Reader &from)
{
    //qDebug() << "StateAnimator: deserializing" << this;

    d->pendingAnimForNode.clear();

    ModelDrawable::Animator::operator << (from);

    from >> d->currentStateName;

    Record storedNames;
    from >> storedNames;

    Record const oldNames = d->names;
    try
    {
        // Initialize matching variables with new values, and add variables that are not
        // found in the current state.
        d->names.assignPreservingVariables(storedNames, Record::IgnoreDoubleUnderscoreMembers);

        // Now that some variables have been deserialized, the AnimationValue objects
        // have been changed.
        d->updateAnimationValuePointers();
    }
    catch (...)
    {
        // Restore valid state.
        d->names = oldNames;
        d->updateAnimationValuePointers();
        throw;
    }
}

} // namespace render
