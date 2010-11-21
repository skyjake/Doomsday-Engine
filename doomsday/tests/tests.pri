# Common setup for the test applications.

QT += gui network

macx {
    # Compile against the deng2 framework.
    INCLUDEPATH += $$(HOME)/Library/Frameworks/deng2.framework/Headers

    QMAKE_LFLAGS += -F$$(HOME)/Library/Frameworks
    LIBS += -framework deng2

    # Configuration files.
    DENG2_CFG_FILES.files = ../../libdeng2/config
    DENG2_CFG_FILES.path = Contents/Resources
    TESTAPP_CFG_FILES.files = ../testapp.de
    TESTAPP_CFG_FILES.path = Contents/Resources/config
    QMAKE_BUNDLE_DATA += DENG2_CFG_FILES TESTAPP_CFG_FILES
}
