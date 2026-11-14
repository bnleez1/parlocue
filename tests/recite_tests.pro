QT += core gui quick quickcontrols2 testlib dbus multimedia
CONFIG += c++17 testcase
TARGET = recite_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/backend.h \
    ../src/filepicker.h \
    ../src/portalfilepicker.h \
    ../src/theme.h \
    ../src/translator.h

SOURCES += \
    recite_tests.cpp \
    ../src/backend.cpp \
    ../src/portalfilepicker.cpp \
    ../src/theme.cpp \
    ../src/translator.cpp

RESOURCES += ../src/resources.qrc
