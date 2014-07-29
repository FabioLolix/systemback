QT += core
QT -= gui

TARGET = sbsudo

CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    sbsudo.cpp

HEADERS += sbsudo.hpp

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback

INCLUDEPATH = ../libsystemback
