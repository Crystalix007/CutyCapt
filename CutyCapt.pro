QT       +=  webengine svg network
SOURCES   =  CutyCapt.cpp
HEADERS   =  CutyCapt.hpp
CONFIG   +=  qt console

greaterThan(QT_MAJOR_VERSION, 4): {
  QT       +=  webenginewidgets printsupport
}

contains(CONFIG, static): {
  QTPLUGIN += qjpeg qgif qsvg qmng qico qtiff
  DEFINES  += STATIC_PLUGINS
}

