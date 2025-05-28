TEMPLATE = app
#CONFIG += c++11
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -std=c++11

SOURCES += \
    Device.cpp \
    main.cpp \
    Buffer.cpp \
    udpstreamer.cpp

HEADERS += \
    Device.h \
    Buffer.h \
    udpstreamer.h

LIBS += \
    -lasound \
    -pthread
