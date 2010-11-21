TEMPLATE = app
TARGET = test_vectors
QT += network

SOURCES += main.cpp

macx {
    INCLUDEPATH += $$(HOME)/Library/Frameworks/deng2.framework/Headers

    QMAKE_LFLAGS += -F$$(HOME)/Library/Frameworks
    LIBS += -framework deng2
}
