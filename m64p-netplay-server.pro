QT -= gui
QT += network websockets

CONFIG += console
CONFIG -= app_bundle

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00

SOURCES += \
        main.cpp \
        udpServer.cpp \
        tcpServer.cpp \
        serverThread.cpp \
        socketServer.cpp

HEADERS += \
        udpServer.h \
        tcpServer.h \
        serverThread.h \
        socketServer.h

!win32 {
CONFIG += ltcg
}

QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -O3 -march=x86-64-v3
QMAKE_CFLAGS_RELEASE -= -O2
QMAKE_CFLAGS_RELEASE += -O3 -march=x86-64-v3
QMAKE_LFLAGS_RELEASE -= -O2
QMAKE_LFLAGS_RELEASE -= -Wl,-O1
QMAKE_LFLAGS_RELEASE += -O3 -march=x86-64-v3

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
