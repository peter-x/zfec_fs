TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
DEFINES += _FILE_OFFSET_BITS=64
LIBS += -lfuse
SOURCES += main.cpp fec.c \
    zfecfsencoder.cpp \
    encodedfile.cpp \
    zfecfsdecoder.cpp \
    decodedfile.cpp
CCFLAG += --std=c11
HEADERS += \
    decodedpath.h \
    utils.h \
    zfecfs.h \
    zfecfsencoder.h \
    encodedfile.h \
    fecwrapper.h \
    metadata.h \
    zfecfsdecoder.h \
    decodedfile.h \
    mutex.h \
    directory.h \
    file.h

