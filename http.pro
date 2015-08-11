QT += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = http
TEMPLATE = app

HEADERS += httpwindow.h
SOURCES += httpwindow.cpp \
           main.cpp
FORMS += dialog.ui

# install
# target.path = $$[QT_INSTALL_EXAMPLES]/network/http
# INSTALLS += target
