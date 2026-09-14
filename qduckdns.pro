QT = core network

CONFIG += c++17 cmdline

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        src/duckdnscore.cpp \
        src/main.cpp

TRANSLATIONS += \
    lang/qduckdns_en_US.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    src/duckdnscore.h \
    src/version.h

DISTFILES += \
    qduckdns.service
CONFIGFILES += \
    qduckdns.cfg

myfiles.files = $$DISTFILES
cfgfiles.files = $$CONFIGFILES
unix:!android:
    myfiles.path = /usr/lib/systemd/system/
    cfgfiles.path = /etc/qduckdns/
    INSTALLS += \
        myfiles \
        cfgfiles
