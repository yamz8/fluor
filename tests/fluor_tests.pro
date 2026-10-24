QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = fluor_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/filepicker.h ../src/panel.h ../src/spectra.h ../src/theme.h ../src/translator.h
SOURCES += fluor_tests.cpp ../src/backend.cpp ../src/panel.cpp ../src/spectra.cpp ../src/theme.cpp ../src/translator.cpp
