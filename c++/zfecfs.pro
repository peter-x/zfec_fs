TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
DEFINES += _FILE_OFFSET_BITS=64
LIBS += -lboost_system # TODO can we get rid of this?
SOURCES += fec.c \
    zfecfsencoder.cpp \
    zfecfsdecoder.cpp \
    fileencoder.cpp \
    filedecoder.cpp \
    metadata.cpp
CCFLAG += --std=c11 -O3
HEADERS += \
    fec.h \
    decodedpath.h \
    utils.h \
    zfecfs.h \
    zfecfsencoder.h \
    fecwrapper.h \
    metadata.h \
    zfecfsdecoder.h \
    directory.h \
    file.h \
    threadlocalizer.h \
    fileencoder.h \
    filedecoder.h

test {
    SOURCES += test/unittest.cpp
    HEADERS += test/testfile.h
    DEFINES += BOOST_TEST_MAIN BOOST_TEST_DYN_LINK
    TARGET = zfecfs_unittest
} else {
    SOURCES += main.cpp
    LIBS += -lfuse
}
