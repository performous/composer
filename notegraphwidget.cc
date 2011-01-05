/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**	 notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**	 notice, this list of conditions and the following disclaimer in
**	 the documentation and/or other materials provided with the
**	 distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**	 the names of its contributors may be used to endorse or promote
**	 products derived from this software without specific prior written
**	 permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtGui>

#include "notelabel.hh"
#include "notegraphwidget.hh"
#include <iostream>
#include <cmath>

NoteGraphWidget::NoteGraphWidget(QWidget *parent)
	: QWidget(parent)
{
	QFile dictionaryFile(":/dictionary/words.txt");
	dictionaryFile.open(QFile::ReadOnly);
	QTextStream inputStream(&dictionaryFile);

	int x = 5;
	int y = 5;

	while (!inputStream.atEnd()) {
		QString word;
		inputStream >> word;
		if (!word.isEmpty()) {
			NoteLabel *wordLabel = new NoteLabel(word, this);
			wordLabel->move(x, y);
			wordLabel->show();
			wordLabel->setAttribute(Qt::WA_DeleteOnClose);
			x += wordLabel->width() + 2;
			if (x >= 245) {
				x = 5;
				y += wordLabel->height() + 2;
			}
		}
	}

	setAcceptDrops(true);
}

void NoteGraphWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("application/x-fridgemagnet")) {
		if (children().contains(event->source())) {
			event->setDropAction(Qt::MoveAction);
			event->accept();
		} else {
			event->acceptProposedAction();
		}
	} else if (event->mimeData()->hasText()) {
		event->acceptProposedAction();
	} else {
		event->ignore();
	}
}

void NoteGraphWidget::dragMoveEvent(QDragMoveEvent *event)
{
	if (event->mimeData()->hasFormat("application/x-fridgemagnet")) {
		if (children().contains(event->source())) {
			event->setDropAction(Qt::MoveAction);
			event->accept();
		} else {
			event->acceptProposedAction();
		}
	} else if (event->mimeData()->hasText()) {
		event->acceptProposedAction();
	} else {
		event->ignore();
	}
}

void NoteGraphWidget::dropEvent(QDropEvent *event)
{
	if (event->mimeData()->hasFormat("application/x-fridgemagnet")) {
		const QMimeData *mime = event->mimeData();
		QByteArray itemData = mime->data("application/x-fridgemagnet");
		QDataStream dataStream(&itemData, QIODevice::ReadOnly);

		QString text;
		QPoint offset;
		dataStream >> text >> offset;
		NoteLabel *newLabel = new NoteLabel(text, this);
		newLabel->move(event->pos() - offset);
		newLabel->show();
		newLabel->setAttribute(Qt::WA_DeleteOnClose);

		if (event->source() == this) {
			event->setDropAction(Qt::MoveAction);
			event->accept();
		} else {
			event->acceptProposedAction();
		}
	} else if (event->mimeData()->hasText()) {
		QStringList pieces = event->mimeData()->text().split(QRegExp("\\s+"),
							 QString::SkipEmptyParts);
		QPoint position = event->pos();

		foreach (QString piece, pieces) {
			NoteLabel *newLabel = new NoteLabel(piece, this);
			newLabel->move(position);
			newLabel->show();
			newLabel->setAttribute(Qt::WA_DeleteOnClose);

			position += QPoint(newLabel->width(), 0);
		}

		event->acceptProposedAction();
	} else {
		event->ignore();
	}
}

void NoteGraphWidget::mousePressEvent(QMouseEvent *event)
{
	NoteLabel *child = static_cast<NoteLabel*>(childAt(event->pos()));
	if (!child)
		return;

	if (event->button() == Qt::LeftButton) {
		QPoint hotSpot = event->pos() - child->pos();

		QByteArray itemData;
		QDataStream dataStream(&itemData, QIODevice::WriteOnly);
		dataStream << child->getText() << QPoint(hotSpot);

		QMimeData *mimeData = new QMimeData;
		mimeData->setData("application/x-fridgemagnet", itemData);
		mimeData->setText(child->getText());

		QDrag *drag = new QDrag(this);
		drag->setMimeData(mimeData);
		drag->setPixmap(*child->pixmap());
		drag->setHotSpot(hotSpot);

		child->hide();

		if (drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::CopyAction) == Qt::MoveAction)
			child->close();
		else
			child->show();

	} else if (event->button() == Qt::RightButton) {
		int cutpos = int(std::ceil(child->getText().length() / 2.0));
		QString firstst = child->getText().left(cutpos);
		QString secondst = child->getText().right(child->getText().length() - cutpos);

		NoteLabel *newLabel1 = new NoteLabel(firstst, this);
		newLabel1->move(child->pos());
		newLabel1->show();
		newLabel1->setAttribute(Qt::WA_DeleteOnClose);

		NoteLabel *newLabel2 = new NoteLabel(secondst, this);
		newLabel2->move(newLabel1->pos() + QPoint(newLabel1->width(), 0));
		newLabel2->show();
		newLabel2->setAttribute(Qt::WA_DeleteOnClose);

		child->close();
	}
}

void NoteGraphWidget::wheelEvent(QWheelEvent *event)
{
	NoteLabel *child = static_cast<NoteLabel*>(childAt(event->pos()));
	if (!child)
		return;

	int neww = child->size().width() + event->delta() * 0.1;
	child->resize(neww, child->size().height());

	std::cout << "RESIZE: " << neww << std::endl;

	event->accept();
}

void NoteGraphWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	NoteLabel *child = static_cast<NoteLabel*>(childAt(event->pos()));
	if (!child)
		return;

	bool ok;
	QString text = QInputDialog::getText(this, tr("Edit lyric"),
										  tr("Lyric:"), QLineEdit::Normal,
										  child->getText(), &ok);
	if (ok && !text.isEmpty()) {
		child->setText(text);
		child->createPixmap();
	}

	event->accept();
}
