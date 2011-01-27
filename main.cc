#include <QApplication>
#include <QTranslator>
#include "config.hh"
#include "editorapp.hh"

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(editor);

	QApplication app(argc, argv);
	app.setApplicationName(PACKAGE);
	app.setApplicationVersion(VERSION);
	app.setOrganizationName("Performous Team");
	app.setOrganizationDomain("performous.org");

	QString locale = QLocale::system().name();
	QTranslator translator;
	translator.load(locale);
	app.installTranslator(&translator);

	EditorApp window;
	window.show();

	return app.exec();
}
