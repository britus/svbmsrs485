QT += core
QT += gui
QT += widgets
QT += network
QT += charts
QT += printsupport
QT += concurrent
QT += serialbus
QT += serialport
QT += bluetooth
QT += location
QT += dbus
QT += xml

CONFIG += c++17
CONFIG += sdk_no_version_check
CONFIG += nostrip
CONFIG += debug
#CONFIG += lrelease
CONFIG += embed_translations
CONFIG += create_prl
CONFIG += app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
	/usr/local/include

QMAKE_LIBDIR += /usr/local/lib

LIBS += -lpiplatesio

SOURCES += \
	cssupervoltbmsdevice.cpp \
	main.cpp \
	mainwindow.cpp

HEADERS += \
	cssupervoltbmsdevice.h \
	mainwindow.h

FORMS += \
	mainwindow.ui

TRANSLATIONS += \
	BMSSuperVolt_en_US.ts

# Default rules for deployment.
target.path = /usr/local/bin
INSTALLS += target

message("Install: $${INSTALLS}")
