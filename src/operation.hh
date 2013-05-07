#pragma once
#include <QString>
#include <QStack>
#include <QVariant>
#include <QTextStream>
#include <QDataStream>
#include <ostream>
#include <stdexcept>

struct Operation
{
	enum OperationFlags { NORMAL = 0, NO_EXEC = 1, NO_EMIT = 2, NO_UPDATE = 4, SELECT_NEW = 8 };

	Operation() { }
	Operation(const QString &opString) { *this << opString; }
	Operation(const QString &opString, int id) { *this << opString << id; }
	Operation(const QString &opString, int id, bool state) { *this << opString << id << state; }
	Operation(const QString &opString, const QString &str1, const QString &str2) { *this << opString << str1 << str2; }

	// Functions to add parameters to Operation

	Operation& operator<<(const QString &str) { m_params.push_back(QVariant(str)); return *this; }
	Operation& operator<<(int i) { m_params.push_back(QVariant(i)); return *this; }
	Operation& operator<<(bool b) { m_params.push_back(QVariant(b)); return *this; }
	Operation& operator<<(float f) { m_params.push_back(QVariant(f)); return *this; }
	Operation& operator<<(double d) { m_params.push_back(QVariant(d)); return *this; }
	Operation& operator<<(QVariant q) { m_params.push_back(q); return *this; }

	/// Get the operation id
	QString op() const { return m_params.isEmpty() ? "" : m_params.front().toString(); }
	/// Get parameter count (excluding operation id)
	int paramCount() const { return m_params.size() - 1; }

	/// Overloaded template getter for param at certain position (1-based)
	template<typename T>
	T param(int index) const { validate(index); m_params[index].value<T>(); }

	// Get Operation parameter at certain index (1-based)

	QString s(int index) const { validate(index); return m_params[index].toString(); }
	char c(int index) const { validate(index); return m_params[index].toChar().toLatin1(); }
	int i(int index) const { validate(index); return m_params[index].toInt(); }
	unsigned u(int index) const { validate(index); return m_params[index].toUInt(); }
	bool b(int index) const { validate(index); return m_params[index].toBool(); }
	float f(int index) const { validate(index); return m_params[index].toFloat(); }
	double d(int index) const { validate(index); return m_params[index].toDouble(); }
	QVariant q(int index) const { validate(index); return m_params[index]; }

	/// Array access for modifying param
	QVariant& operator[](int index) { validate(index); return m_params[index]; }

	std::string dump() const {
		QString st;
		QTextStream ts(&st);
		foreach(QVariant qv, m_params)
			ts << qv.toString() << " ";
		return st.toStdString();
	}

	friend QDataStream& operator<<(QDataStream&, const Operation&);
	friend QDataStream& operator>>(QDataStream& stream, Operation& op);

private:
	void validate(int index) const {
		if (index < 0 || index >= m_params.size())
			throw std::runtime_error("Invalid access to operation parameters");
	}

	QList<QVariant> m_params;
};

typedef QStack<Operation> OperationStack;

// Serialization operators
QDataStream& operator<<(QDataStream& stream, const Operation& op);
QDataStream& operator>>(QDataStream& stream, Operation& op);
