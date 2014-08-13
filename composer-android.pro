#-------------------------------------------------
#
# Project created by QtCreator 2014-08-13T10:27:05
#
#-------------------------------------------------

QT       += core gui widgets multimedia xml

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = composer-android
TEMPLATE = app


SOURCES += \
    src/editorapp.cc \
    src/main.cc \
    src/midifile.cc \
    src/notegraphwidget.cc \
    src/notelabel.cc \
    src/notelabelmanager.cc \
    src/notes.cc \
    src/operation.cc \
    src/pitch.cc \
    src/pitchvis.cc \
    src/song.cc \
    src/songparser.cc \
    src/songparser-ini.cc \
    src/songparser-lrc.cc \
    src/songparser-txt.cc \
    src/songparser-xml.cc \
    src/songwriter-ini.cc \
    src/songwriter-lrc.cc \
    src/songwriter-smm.cc \
    src/songwriter-txt.cc \
    src/songwriter-xml.cc \
    src/synth.cc

HEADERS  += \
    src/busydialog.hh \
    src/config.cmake.hh \
    src/editorapp.hh \
    src/gettingstarted.hh \
    src/midifile.hh \
    src/notegraphwidget.hh \
    src/notelabel.hh \
    src/notes.hh \
    src/operation.hh \
    src/pitch.hh \
    src/pitchvis.hh \
    src/song.hh \
    src/songparser.hh \
    src/songwriter.hh \
    src/synth.hh \
    src/textcodecselector.hh \
    src/types.hh \
    src/util.hh \
    src/libda/fft.hpp \
    src/libda/sample.hpp

FORMS    += \
    ui/aboutdialog.ui \
    ui/editor.ui \
    ui/gettingstarted.ui

CONFIG += mobility
MOBILITY = 

OTHER_FILES += \
    src/CMakeLists.txt \
    icons/application-exit.png \
    icons/composer.png \
    icons/document-new.png \
    icons/document-open.png \
    icons/document-save.png \
    icons/document-save-as.png \
    icons/edit-copy.png \
    icons/edit-cut.png \
    icons/edit-delete.png \
    icons/edit-paste.png \
    icons/edit-redo.png \
    icons/edit-select-all.png \
    icons/edit-undo.png \
    icons/help-about.png \
    icons/help-contents.png \
    icons/help-hint.png \
    icons/insert-object.png \
    icons/insert-text.png \
    icons/media-playback-pause.png \
    icons/media-playback-start.png \
    icons/media-playback-stop.png \
    icons/preferences-other.png \
    icons/zoom-in.png \
    icons/zoom-original.png \
    icons/zoom-out.png \
    icons/composer.svg \
    docs/helpindex.html \
    docs/Design.txt \
    docs/FileFormats.txt \
    docs/License.txt

RESOURCES += \
    editor.qrc

