include(../3rdparty/qtiocompressor/qtiocompressor.pri)
include(../3rdparty/zlib/zlib.pri)

FORMS += ./preferences/generalpage.ui \
        ./preferences/parserpage.ui \
        ./preferences/pathmachinepage.ui \
        ./mainwindow/roomeditattrdlg.ui \
        ./mainwindow/infomarkseditdlg.ui \
        ./mainwindow/findroomsdlg.ui \
        ./preferences/groupmanagerpage.ui \
        ./mainwindow/aboutdialog.ui

HEADERS += ./global/defs.h \
          ./mapdata/roomselection.h \
          ./mapdata/mapdata.h \
          ./mapdata/mmapper2room.h \
          ./mapdata/mmapper2exit.h \
          ./mapdata/infomark.h \
          ./mapdata/customaction.h \
          ./mapdata/roomfactory.h \
          ./proxy/telnetfilter.h \
          ./proxy/proxy.h \
          ./proxy/websocketproxy.h \
          ./proxy/tcpsocketproxy.h \
          ./proxy/connectionlistener.h \
          ./parser/patterns.h \
          ./parser/parser.h \
          ./parser/mumexmlparser.h \
          ./parser/abstractparser.h \
          ./preferences/configdialog.h \
          ./preferences/generalpage.h \
          ./preferences/parserpage.h \
          ./preferences/pathmachinepage.h \
          ./preferences/ansicombo.h \
          ./mainwindow/mainwindow.h \
          ./mainwindow/roomeditattrdlg.h \
          ./mainwindow/infomarkseditdlg.h \
          ./mainwindow/findroomsdlg.h \
          ./mainwindow/aboutdialog.h \
          ./configuration/configuration.h \
          ./display/mapcanvas.h \
          ./display/mapwindow.h \
          ./display/connectionselection.h \
          ./display/prespammedpath.h \
          ./expandoracommon/room.h \
          ./expandoracommon/frustum.h \
          ./expandoracommon/component.h \
          ./expandoracommon/coordinate.h \
          ./expandoracommon/listcycler.h \
          ./expandoracommon/abstractroomfactory.h \
          ./expandoracommon/mmapper2event.h \
          ./expandoracommon/parseevent.h \
          ./expandoracommon/property.h \
          ./expandoracommon/roomadmin.h \
          ./expandoracommon/roomrecipient.h \
          ./expandoracommon/exit.h \
          ./pathmachine/approved.h \
          ./pathmachine/experimenting.h \
          ./pathmachine/pathmachine.h \
          ./pathmachine/path.h \
          ./pathmachine/pathparameters.h \
          ./pathmachine/mmapper2pathmachine.h \
          ./pathmachine/roomsignalhandler.h \
          ./pathmachine/onebyone.h \
          ./pathmachine/crossover.h \
          ./pathmachine/syncing.h \
          ./mapfrontend/intermediatenode.h \
          ./mapfrontend/map.h \
          ./mapfrontend/mapaction.h \
          ./mapfrontend/mapfrontend.h \
          ./mapfrontend/roomcollection.h \
          ./mapfrontend/roomlocker.h \
          ./mapfrontend/roomoutstream.h \
          ./mapfrontend/searchtreenode.h \
          ./mapfrontend/tinylist.h \
          ./mapstorage/mapstorage.h \
          ./mapstorage/abstractmapstorage.h \
          ./mapstorage/basemapsavefilter.h \
          ./mapstorage/filesaver.h \
          ./mapstorage/oldroom.h \
          ./mapstorage/olddoor.h \
          ./mapstorage/roomsaver.h \
          ./mapstorage/oldconnection.h \
          ./mapstorage/progresscounter.h \
          ./parser/roompropertysetter.h \
	  ./pandoragroup/CGroup.h \
	  ./pandoragroup/CGroupChar.h \
	  ./pandoragroup/CGroupClient.h \
	  ./pandoragroup/CGroupCommunicator.h \
	  ./pandoragroup/CGroupServer.h \
	  ./pandoragroup/CGroupStatus.h \
          ./preferences/groupmanagerpage.h \
          ./proxy/websocketcanvas.h

SOURCES += main.cpp \
          ./mapdata/mapdata.cpp \
          ./mapdata/mmapper2room.cpp \
          ./mapdata/mmapper2exit.cpp \
          ./mapdata/roomfactory.cpp \
          ./mapdata/roomselection.cpp \
          ./mapdata/customaction.cpp \
          ./mainwindow/mainwindow.cpp \
          ./mainwindow/roomeditattrdlg.cpp \
          ./mainwindow/infomarkseditdlg.cpp \
          ./mainwindow/findroomsdlg.cpp \
          ./mainwindow/aboutdialog.cpp \
          ./display/mapwindow.cpp \
          ./display/mapcanvas.cpp \
          ./display/connectionselection.cpp \
          ./display/prespammedpath.cpp \
          ./preferences/configdialog.cpp \
          ./preferences/generalpage.cpp \
          ./preferences/parserpage.cpp \
          ./preferences/pathmachinepage.cpp \
          ./preferences/ansicombo.cpp \
          ./configuration/configuration.cpp \
          ./proxy/connectionlistener.cpp \
          ./proxy/telnetfilter.cpp \
          ./proxy/proxy.cpp \
          ./proxy/websocketproxy.cpp \
          ./proxy/tcpsocketproxy.cpp \
          ./parser/parser.cpp \
          ./parser/mumexmlparser.cpp \
          ./parser/abstractparser.cpp \
          ./parser/patterns.cpp \
	  ./parser/roompropertysetter.cpp \
          ./expandoracommon/component.cpp \
          ./expandoracommon/coordinate.cpp \
          ./expandoracommon/frustum.cpp \
          ./expandoracommon/property.cpp \
          ./expandoracommon/mmapper2event.cpp \
          ./expandoracommon/parseevent.cpp \
          ./mapfrontend/intermediatenode.cpp \
          ./mapfrontend/map.cpp \
          ./mapfrontend/mapaction.cpp \
          ./mapfrontend/mapfrontend.cpp \
          ./mapfrontend/roomcollection.cpp \
          ./mapfrontend/roomlocker.cpp \
          ./mapfrontend/searchtreenode.cpp \
          ./pathmachine/approved.cpp \
          ./pathmachine/experimenting.cpp \
          ./pathmachine/pathmachine.cpp \
          ./pathmachine/mmapper2pathmachine.cpp \
          ./pathmachine/roomsignalhandler.cpp \
          ./pathmachine/path.cpp \
          ./pathmachine/pathparameters.cpp \
          ./pathmachine/syncing.cpp \
          ./pathmachine/onebyone.cpp \
          ./pathmachine/crossover.cpp \
          ./mapstorage/roomsaver.cpp \
          ./mapstorage/abstractmapstorage.cpp \
          ./mapstorage/basemapsavefilter.cpp \
          ./mapstorage/filesaver.cpp \
          ./mapstorage/mapstorage.cpp \
          ./mapstorage/oldconnection.cpp \
          ./mapstorage/progresscounter.cpp \
	  ./pandoragroup/CGroup.cpp \
	  ./pandoragroup/CGroupChar.cpp \
	  ./pandoragroup/CGroupClient.cpp \
	  ./pandoragroup/CGroupCommunicator.cpp \
	  ./pandoragroup/CGroupServer.cpp \
	  ./pandoragroup/CGroupStatus.cpp \
          ./preferences/groupmanagerpage.cpp \
    proxy/websocketcanvas.cpp


TARGET = mmapper

DEFINES += MMAPPER_VERSION=\\\"2.3.6\\\"
GIT_COMMIT_HASH = $$system("git log -1 --format=%h")
!isEmpty(GIT_COMMIT_HASH){
    DEFINES += GIT_COMMIT_HASH=\\\"$$GIT_COMMIT_HASH\\\"
}
GIT_BRANCH = $$system("git rev-parse --abbrev-ref HEAD")
!isEmpty(GIT_BRANCH){
    DEFINES += GIT_BRANCH=\\\"$$GIT_BRANCH\\\"
}
windows {
    contains(QMAKE_CC, gcc){
        # MingW
        LIBS += -lopengl32

    }
    contains(QMAKE_CC, cl){
        # Visual Studio
        LIBS += opengl32.lib
    }
}
linux {
    LIBS += -lopengl32
}

RESOURCES += resources/mmapper2.qrc
RC_ICONS = ./resources/win32/m.ico
ICON = ./resources/macosx/m.icns
TEMPLATE = app
DEPENDPATH += .
INCLUDEPATH += . ./global ./mapstorage ./mapdata ./proxy ./parser ./preferences ./configuration ./display ./mainwindow ./expandoracommon ./pathmachine ./mapfrontend ./pandoragroup
QT += opengl network xml gui websockets webenginewidgets
CONFIG += release opengl network xml gui websockets
CONFIG -= debug
RCC_DIR = ../build/resources
UI_DIR = ../build/ui
MOC_DIR = ../build/moc
debug {
    DESTDIR = ../bin/debug
    OBJECTS_DIR = ../build/debug-obj
}
release {
    DESTDIR = ../bin/release
    OBJECTS_DIR = ../build/release-obj
}
