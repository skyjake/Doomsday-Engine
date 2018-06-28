/** @file baseguiapp.cpp  Base class for GUI applications.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/BaseGuiApp"
#include "de/VRConfig"

#include <de/ArrayValue>
#include <de/CommandLine>
#include <de/Config>
#include <de/ConstantRule>
#include <de/DictionaryValue>
#include <de/FileSystem>
#include <de/Function>
#include <de/NativeFont>
#include <de/BaseWindow>
#include <de/ScriptSystem>
#include <de/Font>

namespace de {

static Value *Function_App_LoadFont(Context &, Function::ArgumentValues const &args)
{
    try
    {
        // Try to load the specified font file.
        const String fileName = args.at(0)->asText();
        const Block fontData(App::rootFolder().locate<const File>(fileName));
//        int id;
//        id = QFontDatabase::addApplicationFontFromData(data);
        if (Font::load(fontData))
        {
            LOG_RES_VERBOSE("Loaded font: %s") << fileName;
        }
        else
        {
            LOG_RES_WARNING("Failed to load font: %s") << fileName;
            //qDebug() << args.at(0)->asText();
            //qDebug() << "Families:" << QFontDatabase::applicationFontFamilies(id);
        }
    }
    catch (Error const &er)
    {
        LOG_RES_WARNING("Failed to load font:\n") << er.asText();
    }
    return nullptr;
}

static Value *Function_App_AddFontMapping(Context &, Function::ArgumentValues const &args)
{
    // arg 0: family name
    // arg 1: dictionary with [Text style, Number weight] => Text fontname

    // styles: regular, italic
    // weight: 0-99 (25=light, 50=normal, 75=bold)

    NativeFont::StyleMapping mapping;
    DictionaryValue const &dict = args.at(1)->as<DictionaryValue>();
    DE_FOR_EACH_CONST(DictionaryValue::Elements, i, dict.elements())
    {
        NativeFont::Spec spec;
        ArrayValue const &key = i->first.value->as<ArrayValue>();
        if (key.at(0).asText() == "italic")
        {
            spec.style = NativeFont::Italic;
        }
        spec.weight = roundi(key.at(1).asNumber());
        mapping.insert(spec, i->second->asText());
    }
    NativeFont::defineMapping(args.at(0)->asText(), mapping);
    return nullptr;
}

DE_PIMPL(BaseGuiApp)
, DENG2_OBSERVES(Variable, Change)
{
    Binder binder;
    std::unique_ptr<PersistentState> uiState;
    GLShaderBank shaders;
    WaveformBank waveforms;
    VRConfig vr;
    float windowPixelRatio = 1.0f; ///< Without user's Config.ui.scaleConfig
    ConstantRule *pixelRatio = new ConstantRule;

    ~Impl() override
    {
        releaseRef(pixelRatio);
    }

    void variableValueChanged(Variable &, const Value &) override
    {
        self().setPixelRatio(windowPixelRatio);
    }
};

BaseGuiApp::BaseGuiApp(const StringList &args)
    : GuiApp(args), d(new Impl(this))
{
    d->binder.init(scriptSystem()["App"])
            << DE_FUNC (App_AddFontMapping, "addFontMapping", "family" << "mappings")
            << DE_FUNC (App_LoadFont,       "loadFont",       "fileName");
}

void BaseGuiApp::glDeinit()
{
    GLWindow::glActiveMain();

    d->vr.oculusRift().deinit();
    d->shaders.clear();
}

void BaseGuiApp::initSubsystems(SubsystemInitFlags flags)
{
    GuiApp::initSubsystems(flags);

    // FIXME: (rebasing) This was probably moved elsewhere?

    // The "-dpi" option overrides the detected pixel ratio.
    if (auto dpi = commandLine().check("-dpi", 1))
    {
        d->windowPixelRatio = dpi.params.at(0).toFloat();
    }
    setPixelRatio(d->windowPixelRatio);

    Config::get("ui.scaleFactor").audienceForChange() += d;

    d->uiState.reset(new PersistentState("UIState"));
}

const Rule &BaseGuiApp::pixelRatio() const
{
    return *d->pixelRatio;
}

void BaseGuiApp::setPixelRatio(float pixelRatio)
{
    d->windowPixelRatio = pixelRatio;

    // Apply the overall UI scale factor.
    pixelRatio *= config().getf("ui.scaleFactor", 1.0f);

    if (!fequal(d->pixelRatio->value(), pixelRatio))
    {
        LOG_VERBOSE("Pixel ratio changed to %.1f") << pixelRatio;

        d->pixelRatio->set(pixelRatio);
        scriptSystem()["DisplayMode"].set("PIXEL_RATIO", Value::Number(pixelRatio));
    }
}

BaseGuiApp &BaseGuiApp::app()
{
    return static_cast<BaseGuiApp &>(App::app());
}

PersistentState &BaseGuiApp::persistentUIState()
{
    return *app().d->uiState;
}

GLShaderBank &BaseGuiApp::shaders()
{
    return app().d->shaders;
}

WaveformBank &BaseGuiApp::waveforms()
{
    return app().d->waveforms;
}

VRConfig &BaseGuiApp::vr()
{
    return app().d->vr;
}

void BaseGuiApp::beginNativeUIMode()
{
#if !defined (DE_MOBILE)
    // Switch temporarily to windowed mode. Not needed on macOS because the display mode
    // is never changed on that platform.
    #if !defined (MACOSX)
    {
        auto &win = static_cast<BaseWindow &>(GLWindow::main());
        win.saveState();
        int const windowedMode[] = {
            BaseWindow::Fullscreen, false,
            BaseWindow::End
        };
        win.changeAttributes(windowedMode);
    }
    #endif
#endif
}

void BaseGuiApp::endNativeUIMode()
{
#if !defined (DE_MOBILE)
#   if !defined (MACOSX)
    {
        static_cast<BaseWindow &>(GLWindow::main()).restoreState();
    }
#   endif
#endif
}

} // namespace de
