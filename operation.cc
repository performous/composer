#include "operation.hh"
#include <stdexcept>

namespace {
	static const int MAX_PARAMS = 10;
}

QDataStream& operator<<(QDataStream& stream, const Operation& op)
{
	if (op.m_params.size() > MAX_PARAMS)
		throw std::runtime_error("Operation has too many parameters");
	int i = 0;
	for (i; i < op.m_params.size(); ++i)
		stream << op.m_params[i];
	for (i; i < MAX_PARAMS; ++i)
		stream << QVariant(0);
	return stream;
}

QDataStream& operator>>(QDataStream& stream, Operation& op)
{
	for (int i = 0; i < MAX_PARAMS; ++i ) {
		QVariant qv;
		stream >> qv;
		op << qv;
	}
	return stream;
}
