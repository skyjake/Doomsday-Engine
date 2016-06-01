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
#include "render/shadervar.h"
#include "clientapp.h"
#include "dd_loop.h"

#include <doomsday/world/thinkerdata.h>

#include <de/ScriptedInfo>
#include <de/ScriptSystem>
#include <de/RecordValue>
#include <de/NoneValue>
#include <de/NativeValue>
#include <de/GLUniform>

using namespace de;

namespace render {

static String const DEF_PROBABILITY   ("prob");
static String const DEF_NODE          ("node");
static String const DEF_LOOPING       ("looping");
static String const DEF_PRIORITY      ("priority");
static String const DEF_ALWAYS_TRIGGER("alwaysTrigger");
static String const DEF_RENDER        ("render");
static String const DEF_PASS          ("pass");
static String const DEF_VARIABLE      ("variable");
static String const DEF_ENABLED       ("enabled");
static String const DEF_MATERIAL      ("material");
static String const DEF_ANIMATION     ("animation");
static String const DEF_SPEED         ("speed");
static String const DEF_ANGLE         ("angle");
static String const DEF_AXIS          ("axis");

static String const VAR_ID            ("ID");
static String const VAR_ASSET         ("ASSET");
static String const VAR_ENABLED       ("enabled");
static String const VAR_MATERIAL      ("material");

static String const PASS_GLOBAL       ("");
static String const DEFAULT_MATERIAL  ("default");

static int const ANIM_DEFAULT_PRIORITY = 1;

DENG2_PIMPL(StateAnimator)
, DENG2_OBSERVES(Variable, Change)
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
            if (looping == Looping)
            {
                // Looping animations are always running.
                return true;
            }
            return !atEnd();
        }

        static Sequence *make() { return new Sequence; }
    };

    QHash<String, Sequence> pendingAnimForNode;
    String currentStateName;
    Record names; ///< Local context for scripts, i.e., per-object model state.

    ModelDrawable::Appearance appearance;

    // Lookups used when drawing or updating state:
    QHash<String, int> indexForPassName;
    QHash<Variable *, int> passForMaterialVariable;

    QHash<String, ShaderVars *> passVars;

    struct AnimVar
    {
        Animation angle { 0, Animation::Linear };
        /// Units per second; added to value independently of its animation.
        Animation speed { 0, Animation::Linear };
        Vector3f axis;
    };
    typedef QHash<String, AnimVar *> AnimVars;
    AnimVars animVars;

    Instance(Public *i, DotPath const &id) : Base(i)
    {
        names.add(Record::VAR_NATIVE_SELF).set(new NativeValue(&self)).setReadOnly();
        names.addSuperRecord(ScriptSystem::builtInClass(QStringLiteral("Render"),
                                                        QStringLiteral("StateAnimator")));
        names.addText(VAR_ID, id).setReadOnly();
        names.add(VAR_ASSET).set(new RecordValue(App::asset(id).accessedRecord())).setReadOnly();

        initVariables();

        // Set up the model drawing parameters.
        if (!self.model().passes.isEmpty())
        {
            appearance.drawPasses = &self.model().passes;
        }
        appearance.programCallback = [this] (GLProgram &program, ModelDrawable::ProgramBinding binding)
        {
            bindUniforms(program,
                binding == ModelDrawable::AboutToBind? Bind : Unbind);
        };
        appearance.passCallback = [this] (ModelDrawable::Pass const &pass, ModelDrawable::PassState state)
        {
            bindPassUniforms(*self.model().currentProgram(),
                pass.name,
                state == ModelDrawable::PassBegun? Bind : Unbind);
        };
    }

    ~Instance()
    {
        deinitVariables();
    }

    void initVariables()
    {
        int const passCount = self.model().passes.size();

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
        if (def.has(DEF_RENDER))
        {
            Record const &renderBlock = def.subrecord(DEF_RENDER);

            initVariablesForPass(renderBlock);

            // Each rendering pass is represented by a subrecord, named
            // according the to the pass names.
            auto passes = ScriptedInfo::subrecordsOfType(DEF_PASS, renderBlock);
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

        if (def.has(DEF_ANIMATION))
        {
            auto varDefs = ScriptedInfo::subrecordsOfType(DEF_VARIABLE, def.subrecord(DEF_ANIMATION));
            for (String varName : varDefs.keys())
            {
                initAnimationVariable(varName, *varDefs[varName]);
            }
        }

        DENG2_ASSERT(passIndex == passCount);
        DENG2_ASSERT(indexForPassName.size() == passCount);

        updatePassMask();
        updatePassMaterials();
    }

    void initVariablesForPass(Record const &block,
                              String const &passName = PASS_GLOBAL)
    {
        // Each pass has a variable for selecting the material.
        // The default value is optionally specified in the definition.
        Variable &passMaterialVar = names.addText(passName.concatenateMember(VAR_MATERIAL),
                                                  block.gets(DEF_MATERIAL, DEFAULT_MATERIAL));
        passMaterialVar.audienceForChange() += this;
        passForMaterialVariable.insert(&passMaterialVar, self.model().passes.findName(passName));

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
            var->angle = variableDef.getf(DEF_ANGLE, 0.f);
            var->speed = variableDef.getf(DEF_SPEED, 0.f);
            var->axis  = vectorFromValue<Vector3f>(variableDef.get(DEF_AXIS));

            addBinding(variableName.concatenateMember(DEF_ANGLE), var->angle);
            addBinding(variableName.concatenateMember(DEF_SPEED), var->speed);

            animVars.insert(variableDef.gets(DEF_NODE), var.get());
            var.release();

            // The model should now be transformed even without active
            // animation sequences so that the variables are applied.
            self.setFlags(AlwaysTransformNodes);
        }
        catch (Error const &er)
        {
            LOG_GL_WARNING("%s: %s") << ScriptedInfo::sourceLocation(variableDef)
                                     << er.asText();
        }
    }

    void addBinding(String const &varName, Animation &anim)
    {
        names.add(varName)
                .set(new NativeValue(&anim, &ScriptSystem::builtInClass("Animation")))
                .setReadOnly();
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
        auto const &model = self.model();
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
            DENG2_ASSERT(var.name() == VAR_ENABLED);

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
       auto const &model = self.model();
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
            DENG2_ASSERT(indexForPassName.contains(name));
            appearance.passMask.setBit(indexForPassName[name], true);
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
        auto const &modelRenderer = ClientApp::renderSystem().modelRenderer();

        auto const vars = passVars.constFind(passName);
        if (vars != passVars.constEnd())
        {
            for (auto i : vars.value()->members)
            {
                if (!hasDeclaredVariable(modelRenderer.shaderDefinition(program),
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
        return ModelRenderer::identifierFromText(name, [this] (String const &name) {
            return self.model().animationIdForName(name);
        });
    }

    Sequence &start(Sequence const &spec)
    {
        Sequence &anim = self.start(spec.animId, spec.node).as<Sequence>();
        anim.apply(spec);
        if (anim.timeline)
        {
            anim.clock.reset(new Scheduler::Clock(*anim.timeline, &names));
        }
        applyFlagOperation(anim.flags, Sequence::ClampToDuration,
                           anim.looping == Sequence::Looping? UnsetFlags : SetFlags);
        return anim;
    }
};

StateAnimator::StateAnimator(DotPath const &id, Model const &model)
    : ModelDrawable::Animator(model, Instance::Sequence::make)
    , d(new Instance(this, id))
{}

Model const &StateAnimator::model() const
{
    return static_cast<Model const &>(ModelDrawable::Animator::model());
}

void StateAnimator::setOwnerNamespace(Record &names, String const &varName)
{
    d->names.add(varName).set(new RecordValue(names));

    // Call the onInit() function if there is one.
    if (d->names.has(QStringLiteral("ASSET.onInit")))
    {
        Record ns;
        ns.add(QStringLiteral("self")).set(new RecordValue(d->names));
        Process::scriptCall(Process::IgnoreResult, ns,
                            QStringLiteral("self.ASSET.onInit"),
                            "$self");
    }
}

void StateAnimator::triggerByState(String const &stateName)
{
    using Sequence = Instance::Sequence;

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
            if (frand() > chance) continue;

            // Start the animation on the specified node (defaults to root),
            // unless it is already running.
            String const node = seq.def->gets(DEF_NODE, "");
            int animId = d->animationId(seq.name);

            bool const alwaysTrigger = ScriptedInfo::isTrue(*seq.def, DEF_ALWAYS_TRIGGER, false);
            if (!alwaysTrigger)
            {
                // Do not restart running sequences.
                /// @todo Only restart the animation if the current state is not the expected
                /// one (checking the state cycle).
                if (isRunning(animId, node)) continue;
            }

            int const priority = seq.def->geti(DEF_PRIORITY, ANIM_DEFAULT_PRIORITY);

            // Look up the timeline.
            Scheduler *timeline = seq.timeline;
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
                          priority,
                          timeline);

            // Do not override higher-priority animations.
            if (auto *existing = find(node)->maybeAs<Sequence>())
            {
                if (priority < existing->priority)
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
            LOGDEV_GL_WARNING("Failed to start animation \"%s\": %s")
                    << seq.name << er.asText();
            continue;
        }

        /*LOG_GL_XVERBOSE("Mobj %i starting animation: " _E(b))
                << d->names.geti("self.__id__") << seq.name;*/
        break;
    }
}

void StateAnimator::triggerDamage(int points, struct mobj_s const *inflictor)
{
    /*
     * Here we check for the onDamage() function in the asset. The ASSET
     * variable holds a direct pointer to the asset definition, where the
     * function is defined.
     */
    if (d->names.has(QStringLiteral("ASSET.onDamage")))
    {
        /*
         * We need to provide the StateAnimator instance to the script as an
         * argument, because onDamage() is a member of the asset -- when the
         * method is executed, "self" refers to the asset.
         */
        Record ns;
        ns.add(QStringLiteral("self")).set(new RecordValue(d->names));
        Process::scriptCall(Process::IgnoreResult, ns,
                            QStringLiteral("self.ASSET.onDamage"),
                            "$self", points,
                            inflictor? &THINKER_DATA(inflictor->thinker, ThinkerData) :
                                       nullptr);
    }
}

void StateAnimator::startSequence(int animationId, int priority, bool looping,
                                  String const &node)
{
    using Seq = Instance::Sequence;
    d->start(Seq(animationId, node, looping? Seq::Looping : Seq::NotLooping,
                 priority));
}

void StateAnimator::advanceTime(TimeDelta const &elapsed)
{
    ModelDrawable::Animator::advanceTime(elapsed);

    using Sequence = Instance::Sequence;
    bool retrigger = false;

    // Update animation variables values.
    for (auto *var : d->animVars.values())
    {
        var->angle.shift(var->speed * elapsed);

        // Keep the angle in the 0..360 range.
        float varAngle = var->angle;
        if (varAngle > 360)    var->angle.shift(-360);
        else if (varAngle < 0) var->angle.shift(+360);
    }

    for (int i = 0; i < count(); ++i)
    {
        auto &anim = at(i).as<Sequence>();
        ddouble factor = 1.0;
        // TODO: Determine actual time factor.

        // Advance the sequence.
        TimeDelta animElapsed = factor * elapsed;
        anim.time += animElapsed;

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

Vector4f StateAnimator::extraRotationForNode(String const &nodeName) const
{
    auto found = d->animVars.constFind(nodeName);
    if (found != d->animVars.constEnd())
    {
        Instance::AnimVar const &var = *found.value();
        return Vector4f(var.axis, var.angle);
    }
    return Vector4f();
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

} // namespace render
