# The Doomsday Engine Project -- GUI application framework for libdeng2
# Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU Lesser General Public License
# version 3 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/lgpl.html for details.

include(../config.pri)

TEMPLATE = lib
TARGET   = deng_appfw
VERSION  = $$DENG_VERSION

CONFIG += deng_qtgui deng_qtopengl

include(../dep_deng2.pri)
include(../dep_shell.pri)
include(../dep_gui.pri)
include(../dep_opengl.pri)
include(../dep_rift.pri)

DEFINES += __LIBAPPFW__
INCLUDEPATH += include

win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll
}

# Public headers.
publicHeaders(root, \
    include/de/AtlasProceduralImage \
    include/de/AuxButtonWidget \
    include/de/BaseGuiApp \
    include/de/BaseWindow \
    include/de/BlurWidget \
    include/de/ButtonWidget \
    include/de/ChildWidgetOrganizer \
    include/de/ChoiceWidget \
    include/de/CommandWidget \
    include/de/CompositorWidget \
    include/de/DialogContentStylist \
    include/de/DialogWidget \
    include/de/DocumentWidget \
    include/de/DocumentPopupWidget \
    include/de/FoldPanelWidget \
    include/de/FontLineWrapping \
    include/de/framework \
    include/de/GLTextComposer \
    include/de/GridLayout \
    include/de/GridPopupWidget \
    include/de/GuiRootWidget \
    include/de/GuiWidget \
    include/de/InputDialog \
    include/de/IPersistent \
    include/de/LabelWidget \
    include/de/LineEditWidget \
    include/de/LogWidget \
    include/de/MenuWidget \
    include/de/MessageDialog \
    include/de/NotificationWidget \
    include/de/OculusRift \
    include/de/PanelWidget \
    include/de/PersistentState \
    include/de/PopupMenuWidget \
    include/de/PopupWidget \
    include/de/ProceduralImage \
    include/de/ProgressWidget \
    include/de/ScriptCommandWidget \
    include/de/ScrollAreaWidget \
    include/de/SequentialLayout \
    include/de/SignalAction \
    include/de/SliderWidget \
    include/de/Style \
    include/de/TabWidget \
    include/de/TextDrawable \
    include/de/ToggleWidget \
    include/de/Untrapper \
    include/de/VRWindowTransform \
    include/de/WindowSystem \
    include/de/WindowTransform \
    include/de/VariableChoiceWidget \
    include/de/VariableToggleWidget \
    include/de/VRConfig \
    \
    include/de/libappfw.h \
)

publicHeaders(ui, \
    include/de/ui/ActionItem \
    include/de/ui/Data \
    include/de/ui/ImageItem \
    include/de/ui/Item \
    include/de/ui/ListData \
    include/de/ui/Margins \
    include/de/ui/Stylist \
    include/de/ui/SubmenuItem \
    include/de/ui/SubwidgetItem \
    include/de/ui/VariableToggleItem \
    \
    include/de/ui/defs.h \
)

publicHeaders(framework, \
    include/de/framework/actionitem.h \
    include/de/framework/atlasproceduralimage.h \
    include/de/framework/baseguiapp.h \
    include/de/framework/basewindow.h \
    include/de/framework/childwidgetorganizer.h \
    include/de/framework/data.h \
    include/de/framework/dialogcontentstylist.h \
    include/de/framework/fontlinewrapping.h \
    include/de/framework/gltextcomposer.h \
    include/de/framework/gridlayout.h \
    include/de/framework/guirootwidget.h \
    include/de/framework/guiwidget.h \
    include/de/framework/guiwidgetprivate.h \
    include/de/framework/imageitem.h \
    include/de/framework/item.h \
    include/de/framework/ipersistent.h \
    include/de/framework/listdata.h \
    include/de/framework/margins.h \
    include/de/framework/persistentstate.h \
    include/de/framework/proceduralimage.h \
    include/de/framework/sequentiallayout.h \
    include/de/framework/signalaction.h \
    include/de/framework/style.h \
    include/de/framework/stylist.h \
    include/de/framework/submenuitem.h \
    include/de/framework/subwidgetitem.h \
    include/de/framework/textdrawable.h \
    include/de/framework/untrapper.h \
    include/de/framework/variabletoggleitem.h \
    include/de/framework/vrwindowtransform.h \
    include/de/framework/windowsystem.h \
    include/de/framework/windowtransform.h \
)

publicHeaders(vr, \
    include/de/vr/oculusrift.h \
    include/de/vr/vrconfig.h \
)

publicHeaders(widgets, \
    include/de/widgets/auxbuttonwidget.h \
    include/de/widgets/blurwidget.h \
    include/de/widgets/buttonwidget.h \
    include/de/widgets/choicewidget.h \
    include/de/widgets/commandwidget.h \
    include/de/widgets/compositorwidget.h \
    include/de/widgets/dialogwidget.h \
    include/de/widgets/documentwidget.h \
    include/de/widgets/documentpopupwidget.h \
    include/de/widgets/foldpanelwidget.h \
    include/de/widgets/gridpopupwidget.h \
    include/de/widgets/labelwidget.h \
    include/de/widgets/lineeditwidget.h \
    include/de/widgets/logwidget.h \
    include/de/widgets/menuwidget.h \
    include/de/widgets/notificationwidget.h \
    include/de/widgets/panelwidget.h \
    include/de/widgets/popupmenuwidget.h \
    include/de/widgets/popupwidget.h \
    include/de/widgets/progresswidget.h \
    include/de/widgets/scriptcommandwidget.h \
    include/de/widgets/scrollareawidget.h \
    include/de/widgets/sliderwidget.h \
    include/de/widgets/tabwidget.h \
    include/de/widgets/togglewidget.h \
    include/de/widgets/variablechoicewidget.h \
    include/de/widgets/variabletogglewidget.h \
)

publicHeaders(dialogs, \
    include/de/dialogs/inputdialog.h \
    include/de/dialogs/messagedialog.h \
)

# Sources and private headers.
SOURCES += \
    src/baseguiapp.cpp \
    src/basewindow.cpp \
    src/childwidgetorganizer.cpp \
    src/data.cpp \
    src/dialogcontentstylist.cpp \
    src/dialogs/inputdialog.cpp \
    src/dialogs/messagedialog.cpp \
    src/fontlinewrapping.cpp \
    src/gltextcomposer.cpp \
    src/gridlayout.cpp \
    src/guirootwidget.cpp \
    src/guiwidget.cpp \
    src/item.cpp \
    src/listdata.cpp \
    src/margins.cpp \
    src/persistentstate.cpp \
    src/proceduralimage.cpp \
    src/sequentiallayout.cpp \
    src/signalaction.cpp \
    src/style.cpp \
    src/textdrawable.cpp \
    src/untrapper.cpp \
    src/vrwindowtransform.cpp \
    src/vr/oculusrift.cpp \
    src/vr/vrconfig.cpp \
    src/widgets/auxbuttonwidget.cpp \
    src/widgets/blurwidget.cpp \
    src/widgets/buttonwidget.cpp \
    src/widgets/choicewidget.cpp \
    src/widgets/commandwidget.cpp \
    src/widgets/compositorwidget.cpp \
    src/widgets/dialogwidget.cpp \
    src/widgets/documentwidget.cpp \
    src/widgets/documentpopupwidget.cpp \
    src/widgets/foldpanelwidget.cpp \
    src/widgets/gridpopupwidget.cpp \
    src/widgets/labelwidget.cpp \
    src/widgets/lineeditwidget.cpp \
    src/widgets/logwidget.cpp \
    src/widgets/menuwidget.cpp \
    src/widgets/notificationwidget.cpp \
    src/widgets/panelwidget.cpp \
    src/widgets/popupmenuwidget.cpp \
    src/widgets/popupwidget.cpp \
    src/widgets/progresswidget.cpp \
    src/widgets/scriptcommandwidget.cpp \
    src/widgets/scrollareawidget.cpp \
    src/widgets/sliderwidget.cpp \
    src/widgets/tabwidget.cpp \
    src/widgets/togglewidget.cpp \
    src/widgets/variablechoicewidget.cpp \
    src/widgets/variabletogglewidget.cpp \
    src/windowsystem.cpp \
    src/windowtransform.cpp

# Installation ---------------------------------------------------------------

macx {
    linkDylibToBundledLibdeng2(libdeng_appfw)

    doPostLink("install_name_tool -id @executable_path/../Frameworks/libdeng_appfw.1.dylib libdeng_appfw.1.dylib")

    # Update the library included in the main app bundle.
    doPostLink("mkdir -p ../client/Doomsday.app/Contents/Frameworks")
    doPostLink("cp -fRp libdeng_appfw*dylib ../client/Doomsday.app/Contents/Frameworks")
}
else {
    INSTALLS += target
    target.path = $$DENG_LIB_DIR
}

deng_sdk {
    INSTALLS *= target
    target.path = $$DENG_SDK_LIB_DIR
}
