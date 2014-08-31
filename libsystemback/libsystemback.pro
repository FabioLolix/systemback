QT -= gui

TARGET = systemback

CONFIG += c++11

TEMPLATE = lib

DEFINES += SYSTEMBACK_LIBRARY

SOURCES += sblib.cpp

HEADERS += sblib.hpp \
    sblib_global.hpp \
    sbtypedef.hpp

QMAKE_CXXFLAGS_WARN_ON += -Wextra

LIBS += -lmount \
    -lblkid \
    -lparted
