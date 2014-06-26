#-------------------------------------------------
#
# Project created by QtCreator 2014-02-01T17:20:34
#
#-------------------------------------------------

QT -= gui

TARGET = systemback

TEMPLATE = lib

DEFINES += SYSTEMBACK_LIBRARY

SOURCES += sblib.cpp

HEADERS += sblib.h \
    sblib_global.h

LIBS += -lmount \
    -lblkid
