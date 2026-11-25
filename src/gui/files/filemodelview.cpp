// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2010 - 2012 Qualia Digital Solutions.
// Copyright (c) 2026 LAN Messenger.
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStyle>
#include <QPalette>
#include <QFont>
#include <QBrush>
#include <QPen>
#include <QPixmap>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <math.h>

#include "filemodelview.h"
#include <QGuiApplication>
#include "core/definitions.h"

/****************************************************************************
**	Class: FileView
**	Description: Takes care of rendering the item
****************************************************************************/
FileView::FileView(QString id) {
  type = MT_File;
  mode = TM_Send;
  state = TS_Wait;
  fileSize = 0;
  position = 0;
  speed = 0;
  posDisplay = tr("0 bytes");
  speedDisplay = tr("0 bytes/sec");
  timeDisplay = tr("Calculating time");
  if(!id.isNull())
    this->id = id;
}

QSize FileView::sizeHint() const {
  switch(state) {
  case TS_Wait:
  case TS_Confirm:
  case TS_Complete:
  case TS_Cancel:
    return QSize(0, 78);
    break;
  default:
    return QSize(0, 86);
    break;
  }
}

void FileView::paint(QPainter* painter, const QRect& rect, const QPalette& palette, DisplayMode displayMode) const {
  Q_UNUSED(displayMode);
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setRenderHint(QPainter::TextAntialiasing, true);
  painter->translate(rect.x(), rect.y());

  const bool rtl = QGuiApplication::isRightToLeft();
  const int W = rect.width();
  // Maps a left-to-right x/width pair to the mirrored position in RTL.
  auto X = [&](int x, int w) { return rtl ? W - x - w : x; };
  const int textFlags = Qt::AlignAbsolute | (rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;

  const QColor accent(30, 79, 194);
  const QColor ok(30, 150, 90);
  const QColor bad(205, 70, 60);
  const QColor textColor = palette.color(QPalette::WindowText);
  QColor muted = textColor;
  muted.setAlpha(150);
  const QColor track = palette.color(QPalette::Mid);

  // Card
  const QRect card(8, 5, W - 16, rect.height() - 10);
  QColor fill = palette.color(QPalette::Base);
  QColor edge = track;
  edge.setAlpha(120);
  painter->setPen(QPen(edge, 1));
  painter->setBrush(fill);
  painter->drawRoundedRect(QRectF(X(card.x(), card.width()) + 0.5, card.y() + 0.5, card.width() - 1, card.height() - 1), 10, 10);

  // Icon tile
  const QRect tile(X(20, 44), 15, 44, 44);
  QColor tint = accent;
  tint.setAlpha(30);
  painter->setPen(Qt::NoPen);
  painter->setBrush(tint);
  painter->drawRoundedRect(tile, 10, 10);
  painter->drawPixmap(QRect(tile.x() + 6, tile.y() + 6, 32, 32), icon);

  const int textX = 76;
  const int textW = W - textX - 24;
  QFont font(painter->font());
  font.setBold(true);
  painter->setFont(font);
  painter->setPen(textColor);
  QString nameDisplay = (mode == TM_Send) ? tr("To: ") : tr("From: ");
  nameDisplay.append(userName);
  QRect textRect(X(textX, textW), 10, textW, 20);
  painter->drawText(textRect, textFlags, painter->fontMetrics().elidedText(nameDisplay, Qt::ElideRight, textW));

  font.setBold(false);
  painter->setFont(font);
  painter->setPen(muted);
  textRect = QRect(X(textX, textW), 30, textW, 18);
  painter->drawText(textRect, textFlags, painter->fontMetrics().elidedText(fileDisplay, Qt::ElideMiddle, textW));

  bool drawProgress = false;
  QString stateDisplay;
  QColor stateColor = muted;
  switch(state) {
  case TS_Send:
  case TS_Receive:
    stateDisplay = tr("%1 left - %2 of %3 (%4)").arg(timeDisplay, posDisplay, sizeDisplay, speedDisplay);
    drawProgress = true;
    break;
  case TS_Complete:
    stateDisplay = tr("Completed");
    stateColor = ok;
    break;
  case TS_Cancel:
    stateDisplay = tr("Canceled");
    stateColor = bad;
    break;
  case TS_Abort:
    stateDisplay = tr("Interrupted");
    stateColor = bad;
    break;
  default:
    break;
  }

  if(drawProgress) {
    const double frac = fileSize > 0 ? qBound(0.0, (double)position / (double)fileSize, 1.0) : 0.0;
    const QRect bar(X(textX, textW), 52, textW, 6);
    painter->setPen(Qt::NoPen);
    painter->setBrush(track);
    painter->drawRoundedRect(bar, 3, 3);
    const int fw = qRound(bar.width() * frac);
    if(fw > 0) {
      painter->setBrush(accent);
      painter->drawRoundedRect(QRect(rtl ? bar.right() + 1 - fw : bar.x(), bar.y(), fw, bar.height()), 3, 3);
    }
    painter->setPen(accent);
    QFont pf = painter->font();
    pf.setBold(true);
    painter->setFont(pf);
    const QString pct = QString::number(qRound(frac * 100)) + "%";
    painter->drawText(QRect(X(textX, textW), 10, textW, 20), Qt::AlignAbsolute | (rtl ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, pct);
    painter->setFont(font);
  }

  painter->setPen(stateColor);
  textRect = QRect(X(textX, textW), drawProgress ? 60 : 50, textW, 18);
  painter->drawText(textRect, textFlags, painter->fontMetrics().elidedText(stateDisplay, Qt::ElideRight, textW));

  painter->restore();
}

QDataStream& operator<<(QDataStream& out, const FileView& view) {
  out << view.id << qint32(view.mode) << view.filePath << view.userName << view.fileDisplay
      << qint32(view.state) << view.icon;
  return out;
}

QDataStream& operator>>(QDataStream& in, FileView& view) {
  QString id;
  qint32 mode;
  QString filePath;
  QString userName;
  QString fileDisplay;
  qint32 state;
  QPixmap icon;
  in >> id >> mode >> filePath >> userName >> fileDisplay >> state >> icon;
  view = FileView(id);
  // Clamp: a corrupt transfers.lst must not smuggle an invalid enum into SQLite.
  if(mode == int(FileView::TM_Send) || mode == int(FileView::TM_Receive))
    view.mode = (FileView::TransferMode)mode;
  else
    view.mode = FileView::TM_Send;
  view.filePath = filePath;
  view.fileName = QFileInfo(filePath).fileName();
  view.userName = userName;
  // Legacy stream has no userId/fileSize/startTime: fall back to userName/0/now.
  view.userId = userName;
  view.fileSize = 0;
  view.fileDisplay = fileDisplay;
  view.startTime = QDateTime::currentDateTime();
  if(state >= 0 && state < int(FileView::TS_Max))
    view.state = (FileView::TransferState)state;
  else
    view.state = FileView::TS_Complete;
  view.icon = icon;
  return in;
}


/****************************************************************************
**	Class: FileDelegate
**	Description: Delegates rendering and user input
****************************************************************************/
void FileDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  if(index.data().canConvert<FileView>()) {
    FileView fileView = index.data().value<FileView>();

    FileView::DisplayMode mode = FileView::DM_Normal;
    if(option.state & QStyle::State_Selected)
      mode = FileView::DM_Selected;

    fileView.paint(painter, option.rect, option.palette, mode);
    if(mode == FileView::DM_Selected) {
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setPen(QPen(QColor(30, 79, 194), 2));
      painter->setBrush(Qt::NoBrush);
      painter->drawRoundedRect(QRectF(option.rect.adjusted(8, 5, -8, -5)).adjusted(1, 1, -1, -1), 10, 10);
      painter->restore();
    }
  } else {
    QStyledItemDelegate::paint(painter, option, index);
  }
}

QSize FileDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
  if(index.data().canConvert<FileView>()) {
    FileView fileView = index.data().value<FileView>();
    return fileView.sizeHint();
  } else {
    return QStyledItemDelegate::sizeHint(option, index);
  }
}


/****************************************************************************
**	Class: FileModel
**	Description: Handles the underlying data
****************************************************************************/
int FileModel::rowCount(const QModelIndex& index) const {
  Q_UNUSED(index);
  return transferList.count();
}

QVariant FileModel::data(const QModelIndex& index, int role) const {
  if(!index.isValid())
    return QVariant();

  if(index.row() >= transferList.size())
    return QVariant();

  if(role == Qt::DisplayRole || role == Qt::EditRole)
    return QVariant::fromValue(transferList.at(index.row()));
  else
    return QVariant();
}

Qt::ItemFlags FileModel::flags(const QModelIndex& index) const {
  if(!index.isValid())
    return Qt::ItemIsEnabled;

  return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

bool FileModel::insertRows(int position, int rows, const QModelIndex& index) {
  Q_UNUSED(index);
  beginInsertRows(QModelIndex(), position, position + rows - 1);

  for(int row = 0; row < rows; row++) {
    FileView fileView;
    transferList.insert(position, fileView);
  }

  endInsertRows();
  return true;
}

bool FileModel::removeRows(int position, int rows, const QModelIndex& index) {
  Q_UNUSED(index);
  beginRemoveRows(QModelIndex(), position, position + rows - 1);

  for(int row = 0; row < rows; row++)
    transferList.removeAt(position);

  endRemoveRows();
  return true;
}

bool FileModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if(index.isValid() && role == Qt::UserRole) {
    int row = index.row();
    transferList[row] = value.value<FileView>();
    emit dataChanged(index, index);

    return true;
  }

  return false;
}

void FileModel::insertItem(int position, FileView* fileView) {
  this->insertRows(position, 1, QModelIndex());

  QModelIndex index = this->index(position);
  FileView view = *fileView;
  this->setData(index, QVariant::fromValue(view), Qt::UserRole);
}

void FileModel::removeItem(int position) {
  removeRows(position, 1, QModelIndex());
}

FileView* FileModel::item(int position) {
  if(position < 0 || position >= transferList.count())
    return NULL;

  return &transferList[position];
}

FileView* FileModel::item(QString id) {
  for(int index = 0; index < transferList.count(); index++) {
    if(transferList[index].id.compare(id) == 0)
      return &transferList[index];
  }

  return NULL;
}

FileView* FileModel::item(QString id, FileView::TransferMode mode) {
  for(int index = 0; index < transferList.count(); index++) {
    if(transferList[index].id.compare(id) == 0 && transferList[index].mode == mode)
      return &transferList[index];
  }

  return NULL;
}

int FileModel::itemIndex(QString id, FileView::TransferMode mode) {
  for(int index = 0; index < transferList.count(); index++) {
    if(transferList[index].id.compare(id) == 0 && transferList[index].mode == mode)
      return index;
  }

  return -1;
}

void FileModel::itemChanged(int position) {
  if(position < 0 || position >= transferList.count())
    return;

  QModelIndex index = this->index(position);
  emit dataChanged(index, index);
}

void FileModel::loadData(QString filePath) {
  transferList.clear();

  QFile file(filePath);
  if(!file.open(QIODevice::ReadOnly))
    return;

  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_5_15);
  stream >> transferList;

  file.close();
}

void FileModel::saveData(QString filePath) {
  if(transferList.isEmpty())
    return;

  QDir dir = QFileInfo(filePath).dir();
  if(!dir.exists())
    dir.mkpath(dir.absolutePath());

  QFile file(filePath);
  if(!file.open(QIODevice::WriteOnly))
    return;

  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_5_15);
  stream << transferList;

  file.close();
}
