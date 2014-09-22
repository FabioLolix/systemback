QT -= gui

TARGET = systemback

CONFIG += c++11 \
    exceptions_off

TEMPLATE = lib

DEFINES += SYSTEMBACK_LIBRARY

SOURCES += sblib.cpp

HEADERS += sblib.hpp \
    sblib_global.hpp \
    sbtypedef.hpp

RESOURCES += version.qrc

QMAKE_CXXFLAGS += -fno-rtti \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables

QMAKE_CXXFLAGS_WARN_ON += -Wextra

LIBS += -lmount \
    -lblkid \
    -lparted
