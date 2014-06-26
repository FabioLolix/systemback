#-------------------------------------------------
#
# Project created by QtCreator 2014-05-03T23:14:50
#
#-------------------------------------------------

QT  += core

QT  -= gui

TARGET = sbsysupgrade

CONFIG  += console

CONFIG  -= app_bundle

TEMPLATE = app

SOURCES += sbsysupgrade.cpp

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback
