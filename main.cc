#include <QApplication>
#include "editorapp.hh"

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(editor);

	QApplication app(argc, argv);

	EditorApp window;
	window.show();

	return app.exec();
}
