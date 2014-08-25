QT += core gui widgets

TARGET = systemback

CONFIG += c++11

TEMPLATE = app

SOURCES += main.cpp \
    systemback.cpp

HEADERS += systemback.hpp

FORMS += systemback.ui

RESOURCES += resedit.qrc

QMAKE_LFLAGS += -Wl,--rpath=/usr/lib/systemback

LIBS += -L../libsystemback -lsystemback \
    -lcrypt \
    -lX11
