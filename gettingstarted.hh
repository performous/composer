#include "ui_gettingstarted.h"

class GettingStartedDialog: public QDialog, private Ui::GettingStarted
{
	Q_OBJECT
public:
	GettingStartedDialog(QWidget* parent = 0)
		: QDialog(parent)
	{
		setupUi(this);
	}
};
