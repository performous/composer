#include "operation.hh"


QDataStream& operator<<(QDataStream& stream, const Operation& op)
{
	stream << op.m_params;
	return stream;
}

QDataStream& operator>>(QDataStream& stream, Operation& op)
{
	stream >> op.m_params;
	return stream;
}
