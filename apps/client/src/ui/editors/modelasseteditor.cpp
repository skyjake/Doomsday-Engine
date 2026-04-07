/** @file modelasseteditor.cpp  Editor for model asset parameters.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/editors/modelasseteditor.h"
#include "ui/editors/variablegroupeditor.h"
#include "ui/clientwindow.h"
#include "clientapp.h"
#include "clientplayer.h"
#include "world/clientmobjthinkerdata.h"
#include "world/clientworld.h"
#include "world/map.h"
#include "render/rend_main.h"
#include "render/rendersystem.h"
#include "render/modelloader.h"
#include "render/stateanimator.h"
#include "render/playerweaponanimator.h"

#include <doomsday/world/thinkers.h>

#include <de/animationvalue.h>
#include <de/filesystem.h>
#include <de/dialogcontentstylist.h>
#include <de/nativepointervalue.h>
#include <de/numbervalue.h>
#include <de/packageloader.h>
#include <de/regexp.h>
#include <de/scripting/scriptedinfo.h>
#include <de/sequentiallayout.h>
#include <de/textvalue.h>

using namespace de;
using namespace de::ui;

struct PlayData : public Deletable
{
    int mobjId;
    int animationId;

    PlayData(int mobj = 0, int seq = 0)
        : mobjId(mobj)
        , animationId(seq)
    {}
};

DE_GUI_PIMPL(ModelAssetEditor)
, DE_OBSERVES(PackageLoader, Activity)
, DE_OBSERVES(PanelWidget, AboutToOpen)
, public VariableGroupEditor::IOwner
{
    using Group = VariableGroupEditor;

    LabelWidget *assetLabel;
    ChoiceWidget *assetChoice;
    String assetId;
    Package::Asset asset { nullptr };
    LabelWidget *info;
    LabelWidget *instLabel;
    ChoiceWidget *instChoice;
    List<Group *> groups; ///< Generated based on asset subrecords.
    SafeWidgetPtr<ChoiceWidget> animChoice;
    SafeWidgetPtr<SliderWidget> offsetX;
    SafeWidgetPtr<SliderWidget> offsetY;
    SafeWidgetPtr<SliderWidget> offsetZ;

    Impl(Public *i) : Base(i)
    {
        App::packageLoader().audienceForActivity() += this;

        // The contents of the editor will scroll.
        ScrollAreaWidget *container = &self().containerWidget();

        container->add(assetChoice = new ChoiceWidget);
        assetChoice->popup().useInfoStyle();

        container->add(info = new LabelWidget);
        container->add(instChoice = new ChoiceWidget);
        instChoice->setNoSelectionHint(_E(l) "Click to select");
        instChoice->popup().useInfoStyle();
        instChoice->popup().audienceForAboutToOpen() += this;
        instLabel = LabelWidget::newWithText("Instance:", container);

        instChoice->rule()
                .setInput(Rule::Left, instLabel->rule().right())
                .setInput(Rule::Top,  instLabel->rule().top());
    }

    void setOfLoadedPackagesChanged() override
    {
        updateAssetsList();
    }

    const Rule &firstColumnWidthRule() const override { return self().firstColumnWidth(); }
    ScrollAreaWidget &containerWidget() override { return self().containerWidget(); }

    void resetToDefaults(const String &/*settingName*/) override
    {}

    static const char *pluralSuffix(int count, const char *suffix = "s")
    {
        return count != 1? suffix : "";
    }

    bool isWeaponAsset() const
    {
        return assetId.beginsWith("model.weapon.");
    }

    /**
     * Sets the currently active model asset.
     * @param id  Asset identifier.
     */
    void setAsset(const String& id)
    {
        assetId = id;
        asset = App::asset(id);

        // Collect some information about the model asset.
        const render::Model &model = ClientApp::render().modelRenderer()
                .bank().model<render::Model>(id);

        StringList animNames;
        for (int i = 0; i < model.animationCount(); ++i)
        {
            animNames << model.animationName(i);
        }

        StringList meshNames;
        for (int i = 0; i < model.meshCount(); ++i)
        {
            meshNames << model.meshName(i);
        }

        StringList materialNames;
        for (const auto &i : model.materialIndexForName)
        {
            materialNames << i.first;
        }

        setInfoLabelParams(*info);
        String msg = Stringf(_E(Ta)_E(l) "Path: " _E(.)_E(Tb) "%s\n"
                             _E(Ta)_E(l) "%i Mesh%s: " _E(.)_E(Tb) "%s\n"
                             _E(Ta)_E(l) "%i Material%s: " _E(.)_E(Tb) "%s",
                asset.absolutePath("path").c_str(),
                model.meshCount(),
                pluralSuffix(model.meshCount(), "es"),
                String::join(meshNames, ", ").c_str(),
                materialNames.size(),
                pluralSuffix(materialNames.size()),
                String::join(materialNames, ", ").c_str());
        if (!animNames.isEmpty())
        {
            msg += Stringf(
                _E(Ta) _E(l) "\n%i Animation%s: " _E(.) _E(Tb) "%s",
                model.animationCount(),
                pluralSuffix(model.animationCount(), String::join(animNames, ", ").c_str()));
        }
        msg += Stringf(_E(Ta)_E(l) "\nAutoscale: " _E(.)_E(Tb) "%s",
                              asset.gets("autoscale", "False").c_str());
        info->setText(msg);

        instChoice->items().clear();
        clearGroups();
        updateInstanceList();

        if (!instChoice->items().isEmpty())
        {
            instChoice->setSelected(0);
        }

        makeGroups();
        redoLayout();
    }

    void setInfoLabelParams(LabelWidget &label)
    {
        label.setFont("small");
        label.setTextColor("altaccent");
        label.setSizePolicy(ui::Fixed, ui::Expand);
        label.setAlignment(ui::AlignLeft);
        label.set(GuiWidget::Background(style().colors().colorf("altaccent") *
                                        Vec4f(1, 1, 1, .2f)));
        const Rule &maxWidth = rule("sidebar.width");
        label.rule().setInput(Rule::Width, maxWidth);
        label.setMaximumTextWidth(maxWidth);
    }

    void clearGroups()
    {
        for (dsize i = 0; i < animChoice->items().size(); ++i)
        {
            auto &item = animChoice->items().at(i);
            DE_ASSERT(is<NativePointerValue>(item.data()));
            delete item.data().as<NativePointerValue>().nativeObject<PlayData>();
        }

        for (Group *g : groups)
        {
            // Ownership of the optional title and reset button was given
            // to the container, but the container is not being destroyed.
            g->destroyAssociatedWidgets();
            GuiWidget::destroy(g);
        }
        groups.clear();
    }

    int idNumber()
    {
        return instChoice->selectedItem().data().asInt();
    }

    render::StateAnimator *assetAnimator()
    {
        if (instChoice->items().isEmpty())
        {
            return nullptr;
        }

        const int idNum = idNumber();
        if (isWeaponAsset())
        {
            auto &weaponAnim = ClientApp::player(idNum).playerWeaponAnimator();
            if (weaponAnim.hasModel())
            {
                return &weaponAnim.animator();
            }
        }
        else
        {
            if (const mobj_t *mo = ClientApp::world().map().thinkers().mobjById(idNum))
            {
                auto &mobjData = THINKER_DATA(mo->thinker, ClientMobjThinkerData);
                return mobjData.animator();
            }
        }
        return nullptr;
    }

    void makeGroups()
    {
        clearGroups();

        if (render::StateAnimator *anim = assetAnimator())
        {
            Record &ns = anim->objectNamespace();

            // Manual animation controls.
            Group *g = new Group(this, "", "Animations");
            groups << g;
            g->addLabel("Play:");
            animChoice.reset(new ChoiceWidget);
            const render::Model &model = anim->model();
            for (int i = 0; i < model.animationCount(); ++i)
            {
                // NOTE: PlayData objects allocated here; must be deleted manually later.
                // NativePointerValue does not take ownership.
                animChoice->items() << new ChoiceItem(
                    model.animationName(i), NativePointerValue(new PlayData(idNumber(), i)));
            }
            g->addWidget(animChoice);
            animChoice->audienceForUserSelection() += [this](){ self().playAnimation(); };
            g->commit();

            // Positioning.
            g = new Group(this, "", "Position");
            groups << g;
            {
                // Sliders for the offset vector.
                offsetX.reset(new SliderWidget);
                offsetY.reset(new SliderWidget);
                offsetZ.reset(new SliderWidget);

                g->addLabel("Offset X:");
                g->addWidget(offsetX);
                g->addLabel("Offset Y:");
                g->addWidget(offsetY);
                g->addLabel("Offset Z:");
                g->addWidget(offsetZ);

                offsetX->setRange(Ranged(-50, 50), .1);
                offsetY->setRange(Ranged(-50, 50), .1);
                offsetZ->setRange(Ranged(-50, 50), .1);
                offsetX->setPrecision(1);
                offsetY->setPrecision(1);
                offsetZ->setPrecision(1);
                offsetX->setValue(anim->model().offset.x);
                offsetY->setValue(anim->model().offset.y);
                offsetZ->setValue(anim->model().offset.z);

                offsetX->audienceForUserValue() += [this](){ self().updateOffsetVector(); };
                offsetY->audienceForUserValue() += [this](){ self().updateOffsetVector(); };
                offsetZ->audienceForUserValue() += [this](){ self().updateOffsetVector(); };
            }
            g->commit();

            // Animator variables.
            g = makeGroup(*anim, ns, "Variables");
            g->open();
            groups << g;

            // Make a variable group for each subrecord.
            KeyMap<String, Group *> orderedGroups;
            ns.forSubrecords([this, anim, &orderedGroups] (const String &name, Record &rec)
            {
                orderedGroups.insert(name, makeGroup(*anim, rec, name, true));
                return LoopContinue;
            });
            for (const auto &g : orderedGroups)
            {
                groups << g.second;
            }
        }
    }

    static String varLabel(const String &name)
    {
        return _E(m) + name + _E(.) + ":";
    }

    static String mobjItemLabel(thid_t id)
    {
        return Stringf("ID %u " _E(l) "(dist:%i)", id, int(distanceToMobj(id)));
    }

    static coord_t distanceToMobj(thid_t id)
    {
        const mobj_t *mo = Mobj_ById(id);
        if (mo)
        {
            return (Rend_EyeOrigin().xzy() - Vec3d(mo->origin)).length();
        }
        return -1;
    }

    void populateGroup(Group *g, Record &rec, bool descend, const String &namePrefix = "")
    {
        auto names = map<StringList>(
            rec.members(), [](const Record::Members::value_type &v) { return v.first; });
        names.sort();

        for (const String &name : names)
        {
            if (name.beginsWith("__") || name == "ID" || name == "uMapTime")
                continue;

            Variable &var = rec[name];

            Ranged range(0, 2);
            double step = .01;
            int precision = 2;

            /// @todo Hardcoded... -jk

            if (name == "uGlossiness")
            {
                range = Ranged(0, 200);
                step = 1;
                precision = 1;
            }
            if (name == "uReflectionBlur")
            {
                range = Ranged(0, 40);
                step = .1;
                precision = 1;
            }
            if (name == "uSpecular")
            {
                range = Ranged(0, 10);
                step = .1;
                precision = 1;
            }

            String label = namePrefix.concatenateMember(name);

            if (const NumberValue *num = maybeAs<NumberValue>(var.value()))
            {
                if (num->semanticHints().testFlag(NumberValue::Boolean))
                {
                    g->addSpace();
                    g->addToggle(var, _E(m) + label);
                }
                else
                {
                    g->addLabel(varLabel(label));
                    g->addSlider(var, range, step, precision);
                }
            }
            else if (is<TextValue>(var.value()))
            {
                g->addLabel(varLabel(label));
                g->addLineEdit(var);
            }
            else if (is<AnimationValue>(var.value()))
            {
                g->addLabel(varLabel(label));
                g->addSlider(var, range, step, precision);
            }
            else if (descend && is<RecordValue>(var.value()))
            {
                populateGroup(g, var.valueAsRecord(), descend, label);
            }
        }
    }

    Group *makeGroup(render::StateAnimator &animator, Record &rec,
                     String titleText, bool descend = false)
    {
        LabelWidget *info = nullptr;

        // Check for a rendering pass.
        int passIndex = animator.model().passes.findName(titleText);
        if (passIndex >= 0)
        {
            titleText = Stringf("Render Pass \"%s\"", titleText.c_str());

            // Look up the shader.
            const auto &pass = animator.model().passes.at(passIndex);
            auto &modelLoader = ClientApp::render().modelRenderer().loader();
            String shaderName = modelLoader.shaderName(*pass.program);
            const Record &shaderDef = modelLoader.shaderDefinition(*pass.program);

            // Check the variable declarations.
            auto vars =
                map<StringList>(ScriptedInfo::subrecordsOfType(DE_STR("variable"), shaderDef),
                                [](const Record::Subrecords::value_type &v) { return v.first; });
            vars.sort();
            StringList names;
            for (const String &n : vars)
            {
                // Used variables are shown in bold.
                if (rec.has(n))
                    names << _E(b) + n + _E(.);
                else
                    names << n;
            }
            String msg = Stringf(_E(Ta) _E(l) "Shader: "    _E(.) _E(Tb) "%s\n"
                                        _E(Ta) _E(l) "Variables: " _E(.) _E(Tb) "%s",
                                        shaderName.c_str(),
                                        String::join(names, ", ").c_str());

            info = new LabelWidget;
            info->setText(msg);
        }

        std::unique_ptr<Group> g(new Group(this, "", titleText, info));
        g->setResetable(false);
        populateGroup(g.get(), rec, descend);
        g->commit();
        if (info)
        {
            setInfoLabelParams(*info);
        }
        return g.release();
    }

    void redoLayout()
    {
        SequentialLayout &layout = self().layout();
        layout.clear();
        layout << *assetLabel << *info << *instLabel;
        for (Group *g : groups)
        {
            layout << g->title() << *g;
        }
        assetChoice->rule().setLeftTop(assetLabel->rule().right(), assetLabel->rule().top());
        self().updateSidebarLayout(assetLabel->rule().width() + assetChoice->rule().width());
    }

    void updateInstanceList()
    {
        instChoice->items().clear();

        if (!ClientApp::world().hasMap())
            return;

        if (isWeaponAsset())
        {
            for (int idx = 0; idx < ClientApp::players().count(); ++idx)
            {
                auto &client = ClientApp::player(idx);
                auto &anim = client.playerWeaponAnimator();
                if (anim.hasModel())
                {
                    if (anim.animator()["ID"] == assetId)
                    {
                        instChoice->items()
                                << new ChoiceItem(Stringf("Player %i", idx), NumberValue(idx));
                    }
                }
            }
        }
        else
        {
            ClientApp::world().map().thinkers().forAll(0x1 /* public */, [this] (thinker_t *th)
            {
                const auto *mobjData = THINKER_DATA_MAYBE(*th, ClientMobjThinkerData);
                if (mobjData && mobjData->animator())
                {
                    const render::StateAnimator *anim = mobjData->animator();

                    if (anim && (*anim)["ID"] == assetId)
                    {
                        instChoice->items() << new ChoiceItem(mobjItemLabel(th->id), NumberValue(th->id));
                    }
                }
                return LoopContinue;
            });
            sortInstancesByDistance(false);
        }
    }

    void sortInstancesByDistance(bool rememberSelection)
    {
        if (isWeaponAsset() || instChoice->items().isEmpty())
            return;

        int selId = (instChoice->isValidSelection()?
                     instChoice->selectedItem().data().asInt() : 0);

        // Update the distances.
        instChoice->items().forAll([] (Item &a)
        {
            a.as<ChoiceItem>().setLabel(mobjItemLabel(a.data().asInt()));
            return LoopContinue;
        });
        instChoice->items().sort([] (const Item &a, const Item &b)
        {
            return distanceToMobj(a.data().asInt()) < distanceToMobj(b.data().asInt());
        });

        if (rememberSelection && selId > 0)
        {
            instChoice->setSelected(instChoice->items().findData(NumberValue(selId)));
        }
    }

    void panelAboutToOpen(PanelWidget &) override
    {
        sortInstancesByDistance(true);
    }

    void updateAssetsList()
    {
        assetChoice->items().clear();

        FS::locate<Folder const>("/packs").forContents([this] (String name, File &)
        {
            static const RegExp regex(R"(asset\.(model\.((thing|weapon)\..*)))");
            RegExpMatch m;
            if (regex.exactMatch(name, m))
            {
                assetChoice->items() << new ChoiceItem(m.captured(2), TextValue(m.captured(1)));
            }
            return LoopContinue;
        });
    }

    void playSelectedAnimation()
    {
        if (!animChoice || !animChoice->isValidSelection())
            return;

        const PlayData &data =
            *animChoice->selectedItem().data().as<NativePointerValue>().nativeObject<PlayData>();
        render::StateAnimator *animator = nullptr;

        if (isWeaponAsset())
        {
            auto &weapon = ClientApp::player(data.mobjId).playerWeaponAnimator();
            if (weapon.hasModel()) animator = &weapon.animator();
        }
        else
        {
            if (const mobj_t *mo = Mobj_ById(data.mobjId))
            {
                if (auto *thinker = THINKER_DATA_MAYBE(mo->thinker, ClientMobjThinkerData))
                {
                    animator = thinker->animator();
                }
            }
        }

        if (animator)
        {
            animator->startAnimation(data.animationId, 10, false);
        }
    }

    void updateOffsetVector()
    {
        if (!offsetX) return;
        if (render::StateAnimator *anim = assetAnimator())
        {
            auto *model = const_cast<render::Model *>(&anim->model());
            model->offset = Vec3f(offsetX->value(), offsetY->value(), offsetZ->value());
        }
    }
};

ModelAssetEditor::ModelAssetEditor()
    : SidebarWidget("Edit 3D Model", "modelasseteditor")
    , d(new Impl(this))
{
    d->assetLabel = LabelWidget::newWithText("Asset:", &containerWidget());

    d->redoLayout();
    d->updateAssetsList();

    d->assetChoice->audienceForUserSelection() +=
        [this]() { setSelectedAsset(d->assetChoice->selected()); };
    d->instChoice->audienceForUserSelection() +=
        [this]() { setSelectedInstance(d->instChoice->selected()); };
}

void ModelAssetEditor::setSelectedAsset(ui::DataPos pos)
{
    d->setAsset(d->assetChoice->items().at(pos).data().asText());
}

void ModelAssetEditor::setSelectedInstance(ui::DataPos /*pos*/)
{
    d->makeGroups();
    d->redoLayout();
}

void ModelAssetEditor::playAnimation()
{
    d->playSelectedAnimation();
}

void ModelAssetEditor::updateOffsetVector()
{
    d->updateOffsetVector();
}
