include ( ../../mythconfig.mak )
include ( ../../settings.pro )
include ( ../../programs-libs.pro )

QT += xml sql opengl qt3support network

PREFIX=/usr/local
LIBDIR=/usr/local/lib

TEMPLATE = lib
CONFIG += plugin thread
TARGET = mythpianod
target.path = $${LIBDIR}/mythtv/plugins
INSTALLS += target

INCLUDEPATH += $${PREFIX}/include/mythtv
INCLUDEPATH += $${PREFIX}/include/mythtv/libmyth
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythui
INCLUDEPATH += $${PREFIX}/include/mythtv/libmythdb

LIBS += -lmythavformat
LIBS += -lmythavcodec
LIBS += -lmythavutil
LIBS += -lgnutls

# Input
HEADERS += config.h mythpianod.h
SOURCES += main.cpp mythpianod.cpp

include ( ../../libs-targetfix.pro )
