QT += core
QT += network
QT -= gui

TARGET = etherdraw-stresstest
CONFIG += console
CONFIG += debug
CONFIG -= app_bundle

LIBS += -lqjson

TEMPLATE = app

SOURCES += main.cpp

SOURCES += Logger.cpp
HEADERS += Logger.h

SOURCES += Client.cpp
HEADERS += Client.h

SOURCES += Pad.cpp
HEADERS += Pad.h

SOURCES += Changeset.cpp
HEADERS += Changeset.h

SOURCES += XhrClient.cpp
HEADERS += XhrClient.h
