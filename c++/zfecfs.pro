TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
DEFINES += _FILE_OFFSET_BITS=64
LIBS += -lfuse
SOURCES += main.cpp fec.c \
    zfecfsencoder.cpp \
    zfecfsdecoder.cpp \
    fileencoder.cpp \
    filedecoder.cpp
CCFLAG += --std=c11
HEADERS += \
    fec.h \
    decodedpath.h \
    utils.h \
    zfecfs.h \
    zfecfsencoder.h \
    fecwrapper.h \
    metadata.h \
    zfecfsdecoder.h \
    mutex.h \
    directory.h \
    file.h \
    threadlocalizer.h \
    fileencoder.h \
    filedecoder.h

