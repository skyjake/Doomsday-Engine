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

#include "de/baseguiapp.h"
#include "de/vrconfig.h"

#include <de/basewindow.h>
#include <de/commandline.h>
#include <de/config.h>
#include <de/dscript.h>
#include <de/filesystem.h>
#include <de/font.h>
#include <de/nativefont.h>
#include <de/windowsystem.h>

namespace de {

static Value *Function_App_LoadFont(Context &, const Function::ArgumentValues &args)
{
    try
    {
        // Try to load the specified font file.
        const String fileName = args.at(0)->asText();
        const Block fontData(App::rootFolder().locate<const File>(fileName));
        if (Font::load(fileName.fileNameWithoutExtension(), fontData))
        {
            LOG_RES_VERBOSE("Loaded font: %s") << fileName;
        }
        else
        {
            LOG_RES_WARNING("Failed to load font: %s") << fileName;
        }
    }
    catch (const Error &er)
    {
        LOG_RES_WARNING("Failed to load font:\n") << er.asText();
    }
    return nullptr;
}

static Value *Function_App_AddFontMapping(Context &, const Function::ArgumentValues &args)
{
    // arg 0: family name
    // arg 1: dictionary with [Text style, Number weight] => Text fontname

    // styles: regular, italic
    // weight: 0-99 (25=light, 50=normal, 75=bold)

    NativeFont::StyleMapping mapping;
    const DictionaryValue &dict = args.at(1)->as<DictionaryValue>();
    DE_FOR_EACH_CONST(DictionaryValue::Elements, i, dict.elements())
    {
        NativeFont::Spec spec;
        const ArrayValue &key = i->first.value->as<ArrayValue>();
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

DE_PIMPL_NOREF(BaseGuiApp)
{
    Binder binder;
    std::unique_ptr<PersistentState> uiState;
    GLShaderBank shaders;
    WaveformBank waveforms;
    VRConfig vr;
};

BaseGuiApp::BaseGuiApp(const StringList &args)
    : GuiApp(args), d(new Impl)
{
    d->binder.init(scriptSystem()["App"])
            << DE_FUNC (App_AddFontMapping, "addFontMapping", "family" << "mappings")
            << DE_FUNC (App_LoadFont,       "loadFont",       "fileName");
}

void BaseGuiApp::glDeinit()
{
    GLWindow::glActivateMain();
    d->vr.oculusRift().deinit();
    d->shaders.clear();

    windowSystem().closeAll();
}

void BaseGuiApp::initSubsystems(SubsystemInitFlags flags)
{
    GuiApp::initSubsystems(flags);
    d->uiState.reset(new PersistentState("UIState"));
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
    // Switch temporarily to windowed mode. Not needed on macOS because the display mode
    // is never changed on that platform.
    #if !defined (MACOSX)
    {
        auto &win = static_cast<BaseWindow &>(GLWindow::getMain());
        win.saveState();
        int const windowedMode[] = {
            BaseWindow::Fullscreen, false,
            BaseWindow::End
        };
        win.changeAttributes(windowedMode);
    }
    #endif
}

void BaseGuiApp::endNativeUIMode()
{
    auto &win = static_cast<BaseWindow &>(GLWindow::getMain());
#   if !defined (MACOSX)
    {
        win.restoreState();
    }
#   endif
    win.raise();
}

} // namespace de
