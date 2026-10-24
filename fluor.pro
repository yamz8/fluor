QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = fluor
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/panel.h \
    src/portalfilepicker.h \
    src/spectra.h \
    src/spectraplot.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/panel.cpp \
    src/portalfilepicker.cpp \
    src/spectra.cpp \
    src/spectraplot.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
