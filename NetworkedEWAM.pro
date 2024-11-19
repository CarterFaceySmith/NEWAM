QT += core network
QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

TEMPLATE = app
TARGET = ewam-network-tester

HEADERS += \
    ../AbstractNetworkInterface/pe.h \
    ../AbstractNetworkInterface/emitter.h \
    networkedEWAM.h

SOURCES += \
    main.cpp \
    networkedEWAM.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Output directory
DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/build/.obj
MOC_DIR = $$PWD/build/.moc
RCC_DIR = $$PWD/build/.rcc
UI_DIR = $$PWD/build/.ui

# Debug configuration
CONFIG(debug, debug|release) {
    DEFINES += DEBUG
    QMAKE_CXXFLAGS_DEBUG += -O0 -g3 -ggdb
}

# Release configuration
CONFIG(release, debug|release) {
    DEFINES += QT_NO_DEBUG_OUTPUT
    QMAKE_CXXFLAGS_RELEASE += -O3
}

# Platform specific settings
win32 {
    DEFINES += WIN32_LEAN_AND_MEAN
    QMAKE_CXXFLAGS += /MP
}

unix {
    QMAKE_CXXFLAGS += -Wall -Wextra
}

# Version information
VERSION = 1.0.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
