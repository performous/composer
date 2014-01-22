#include <QApplication>
#include <QTranslator>
#include <iostream>
#include "config.hh"
#include "editorapp.hh"

#ifdef STATIC_PLUGINS
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(editor);

	QApplication app(argc, argv);
	// These values are used by e.g. Phonon and QSettings
	app.setApplicationName(PACKAGE);
	app.setApplicationVersion(VERSION);
	app.setOrganizationName("Performous Team");
	app.setOrganizationDomain("performous.org");

	// Command line parsing
	// Unfortunately Qt doesn't include proper interface for this,
	// even though it does it internally (and handles some args).
	// This here is not very elegant, but didn't want to introduce
	// additional dependency for a couple of very simple options.
	QStringList args = QApplication::arguments();
	QString openpath = "";
	for (int i = 1; i < args.size(); ++i) { // FIXME: On Windows arg0 might or might not be the program name
		if (args[i] == "--version" || args[i] == "-v") {
			std::cout << VERSION << std::endl;
			exit(EXIT_SUCCESS);
		}
		else if (args[i] == "--help" || args[i] == "-h") {
			std::cout << PACKAGE << " " << VERSION << std::endl << std::endl
				<< "-h [ --help ]      you are viewing it" << std::endl
				<< "-v [ --version ]   display version number" << std::endl
				<< "argument without a switch is interpreted as a song file to open" << std::endl
				;
			exit(EXIT_SUCCESS);
		}
		else if (!args[i].startsWith("-")) openpath = args[i]; // No switch
		else {
			std::cout << "Unknown option: " << args[i].toStdString() << std::endl;
			exit(EXIT_FAILURE);
		}
	}

	// Localization
	QString locale = QLocale::system().name();
	QTranslator translator;
	translator.load(locale);
	app.installTranslator(&translator);

	EditorApp window;
	window.show();

	if (!openpath.isEmpty()) window.openFile(openpath); // Load song if given in command line

	return app.exec();
}
