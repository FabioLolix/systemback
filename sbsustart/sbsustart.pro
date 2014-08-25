QT += core
QT -= gui

TARGET = sbsustart

CONFIG += console \
    c++11

CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    sbsustart.cpp

HEADERS += sbsustart.hpp

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback

INCLUDEPATH = ../libsystemback
