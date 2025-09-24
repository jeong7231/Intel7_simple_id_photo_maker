QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# OpenCV configuration
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += opencv4
}

win32 {
    INCLUDEPATH += C:/opencv/build/include
    LIBS += -LC:/opencv/build/x64/vc15/lib \
        -lopencv_core \
        -lopencv_imgproc \
        -lopencv_imgcodecs \
        -lopencv_videoio \
        -lopencv_calib3d \
        -lopencv_features2d
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    aspectratiolabel.cpp \
    export_page.cpp \
    main.cpp \
    main_app.cpp \
    photoeditpage.cpp \
    suitcomposer.cpp

HEADERS += \
    aspectratiolabel.h \
    export_page.h \
    main_app.h \
    photoeditpage.h \
    suitcomposer.h

FORMS += \
    export_page.ui \
    main_app.ui \
    photoeditpage.ui

TRANSLATIONS += \
    Simple-Smart-ID-Photo-Maker_Qt_ko_KR.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/${TARGET}/bin
else: unix:!android: target.path = /opt/${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
