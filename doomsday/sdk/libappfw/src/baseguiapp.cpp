/** @file baseguiapp.cpp  Base class for GUI applications.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/DictionaryValue>
#include <de/Function>
#include <de/NativeFont>
#include <de/ScriptSystem>
#include <QFontDatabase>

#ifdef WIN32
#  include <d2d1.h>
#endif

namespace de {

static Value *Function_App_LoadFont(Context &, Function::ArgumentValues const &args)
{
    try
    {
        // Try to load the specific font.
        Block data(App::rootFolder().locate<File const>(args.at(0)->asText()));
        int id;
        id = QFontDatabase::addApplicationFontFromData(data);
        if(id < 0)
        {
            LOG_RES_WARNING("Failed to load font:");
        }
        else
        {
            LOG_RES_VERBOSE("Loaded font: %s") << args.at(0)->asText();
            //qDebug() << args.at(0)->asText();
            //qDebug() << "Families:" << QFontDatabase::applicationFontFamilies(id);
        }
    }
    catch(Error const &er)
    {
        LOG_RES_WARNING("Failed to load font:\n") << er.asText();
    }
    return 0;
}

static Value *Function_App_AddFontMapping(Context &, Function::ArgumentValues const &args)
{
    // arg 0: family name
    // arg 1: dictionary with [Text style, Number weight] => Text fontname

    // styles: regular, italic
    // weight: 0-99 (25=light, 50=normal, 75=bold)

    NativeFont::StyleMapping mapping;

    DictionaryValue const &dict = args.at(1)->as<DictionaryValue>();
    DENG2_FOR_EACH_CONST(DictionaryValue::Elements, i, dict.elements())
    {
        NativeFont::Spec spec;
        ArrayValue const &key = i->first.value->as<ArrayValue>();
        if(key.at(0).asText() == "italic")
        {
            spec.style = NativeFont::Italic;
        }
        spec.weight = roundi(key.at(1).asNumber());
        mapping.insert(spec, i->second->asText());
    }

    NativeFont::defineMapping(args.at(0)->asText(), mapping);

    return 0;
}

DENG2_PIMPL_NOREF(BaseGuiApp)
{
    Binder binder;
    QScopedPointer<PersistentState> uiState;
    GLShaderBank shaders;
    WaveformBank waveforms;
    VRConfig vr;
    double dpiFactor = 1.0;

#ifdef WIN32
    Instance()
    {
        // Use the Direct2D API to find out the desktop DPI factor.
        ID2D1Factory *d2dFactory = nullptr;
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        if(SUCCEEDED(hr))
        {
            FLOAT dpiX = 96;
            FLOAT dpiY = 96;
            d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
            dpiFactor = dpiX / 96.0;
            d2dFactory->Release();
            d2dFactory = nullptr;
        }
    }
#endif
};

BaseGuiApp::BaseGuiApp(int &argc, char **argv)
    : GuiApp(argc, argv), d(new Instance)
{
    // Override the system locale (affects number/time formatting).
    QLocale::setDefault(QLocale("en_US.UTF-8"));

    d->binder.init(scriptSystem().nativeModule("App"))
            << DENG2_FUNC (App_AddFontMapping, "addFontMapping", "family" << "mappings")
            << DENG2_FUNC (App_LoadFont,       "loadFont", "fileName");
}

double BaseGuiApp::dpiFactor() const
{
    return d->dpiFactor;
}

void BaseGuiApp::initSubsystems(SubsystemInitFlags flags)
{
    GuiApp::initSubsystems(flags);

#ifndef WIN32
#  ifdef DENG2_QT_5_0_OR_NEWER
    d->dpiFactor = devicePixelRatio();
#  else
    d->dpiFactor = 1.0;
#  endif
#endif

    // The "-dpi" option overrides the detected DPI factor.
    if(auto dpi = commandLine().check("-dpi", 1))
    {
        d->dpiFactor = dpi.params.at(0).toDouble();
    }

    // Apply the overall UI scale factor.
    d->dpiFactor *= config().getf("ui.scaleFactor", 1.f);

    scriptSystem().nativeModule("DisplayMode").set("DPI_FACTOR", d->dpiFactor);

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

} // namespace de
