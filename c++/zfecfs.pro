TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
DEFINES += _FILE_OFFSET_BITS=64
LIBS += -lfuse
SOURCES += main.cpp fec.c \
    zfecfsencoder.cpp \
    encodedfile.cpp

HEADERS += \
    decodedpath.h \
    utils.h \
    zfecfs.h \
    zfecfsencoder.h \
    encodedfile.h

