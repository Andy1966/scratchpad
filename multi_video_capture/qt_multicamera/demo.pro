QT = widgets
CONFIG += c++14
DEFINES += \
  QT_DEPRECATED_WARNINGS \
  QT_DISABLE_DEPRECATED_BEFORE=0x060000 \
  QT_RESTRICTED_CAST_FROM_ASCII
TEMPLATE = app
SOURCES = main.cpp \
    src-cpp-properties/Properties.cpp \
    src-cpp-properties/PropertiesParser.cpp \
    src-cpp-properties/PropertiesUtils.cpp

linux {
INCLUDEPATH += /usr/local/lib
INCLUDEPATH += /usr/local/include/opencv4
LIBS += -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio
}

windows{

INCLUDEPATH += C:\development\opencv\build\include

LIBS += -L C:/development/opencv/build/bin/ \
        -lopencv_core410 \
        -lopencv_imgproc410 \
        -lopencv_highgui410 \
        -lopencv_videoio410 \
        -lopencv_imgcodecs410
}

macx {
  INCLUDEPATH += /opt/local/include
  LIBS += -L /opt/local/lib
}

HEADERS += \
    include-cpp-properties/Properties.h \
    include-cpp-properties/PropertiesException.h \
    include-cpp-properties/PropertiesParser.h \
    include-cpp-properties/PropertiesUtils.h \
    include-cpp-properties/PropertyNotFoundException.h

DISTFILES += \
    videoProperties.ini
