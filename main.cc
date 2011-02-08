#include <QApplication>
#include <QTranslator>
#include "config.hh"
#include "editorapp.hh"

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(editor);

	QApplication app(argc, argv);
	// These values are used by e.g. Phonon and QSettings
	app.setApplicationName(PACKAGE);
	app.setApplicationVersion(VERSION);
	app.setOrganizationName("Performous Team");
	app.setOrganizationDomain("performous.org");

	// Localization
	QString locale = QLocale::system().name();
	QTranslator translator;
	translator.load(locale);
	app.installTranslator(&translator);

	EditorApp window;
	window.show();

	return app.exec();
}
