#-------------------------------------------------
#
# Project created by QtCreator 2014-01-20T15:48:28
#
#-------------------------------------------------

QT += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = systemback

TEMPLATE = app

SOURCES += main.cpp \
    systemback.cpp

HEADERS += systemback.h

FORMS += systemback.ui

RESOURCES += resedit.qrc

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback \
    -lcrypt
