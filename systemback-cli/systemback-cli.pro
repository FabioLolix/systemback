#-------------------------------------------------
#
# Project created by QtCreator 2014-02-01T17:22:25
#
#-------------------------------------------------

QT  += core

QT  -= gui

TARGET = systemback-cli

CONFIG  += console

CONFIG  -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    systemback.cpp

HEADERS += systemback.h

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback \
    -lncursesw

RESOURCES += \
    resedit.qrc
