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

#include <de/Function>
#include <de/ArrayValue>
#include <de/DictionaryValue>
#include <de/NativeFont>
#include <QFontDatabase>

namespace de {

static Value *Function_App_LoadFont(Context &, Function::ArgumentValues const &args)
{
    try
    {
        // Try to load the specific font.
        Block data(App::fileSystem().root().locate<File const>(args.at(0)->asText()));
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
    VRConfig vr;
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

VRConfig &BaseGuiApp::vr()
{
    return app().d->vr;
}

} // namespace de
