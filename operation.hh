#pragma once
#include <QString>
#include <QStack>

///! This is class is draft and subject to change

struct Operation
{
	enum OperationFlags { NORMAL = 0, NO_EXEC = 1, NO_EMIT = 2 };

	// FIXME: Somekind of nice serializable type for constructor
	Operation(QString opString = "");

	QString owner; /// Who performs the operation
	unsigned id; /// E.g. a child id of the owner
	unsigned action; /// Id of the action to-be-performed
	void *data; /// User data
};

typedef QStack<Operation> OperationStack;
