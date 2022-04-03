QT -= gui
QT += network websockets

CONFIG += optimize_full console
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

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
