QT -= gui

TARGET = systemback

TEMPLATE = lib

DEFINES += SYSTEMBACK_LIBRARY

SOURCES += sblib.cpp

HEADERS += sblib.hpp \
    sblib_global.hpp

LIBS += -lmount \
    -lblkid
