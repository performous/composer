#include <QApplication>
#include "config.hh"
#include "editorapp.hh"

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(editor);

	QApplication app(argc, argv);
	app.setApplicationName(PACKAGE);
	app.setApplicationVersion(VERSION);

	EditorApp window;
	window.show();

	return app.exec();
}
