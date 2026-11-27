// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2010 - 2012 Qualia Digital Solutions.
// Copyright (c) 2026 LAN Messenger.
#include "usertreewidget.h"

#include <QPainterPath>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

lmcUserTreeWidgetItem::lmcUserTreeWidgetItem()
    : QTreeWidgetItem(UserType + 1) {
  //	make item not user checkable
  setFlags(flags() & ~Qt::ItemIsUserCheckable);
}

QRect lmcUserTreeWidgetItem::checkBoxRect(const QRect& itemRect) {
  lmcUserTreeWidget* treeWidget = static_cast<lmcUserTreeWidget*>(this->treeWidget());
  int cbWidth = 12;
  int cbHeight = 12;
  if(treeWidget) {
    QStyle* style = treeWidget->style();
    cbWidth = style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, treeWidget);
    cbHeight = style->pixelMetric(QStyle::PM_IndicatorHeight, nullptr, treeWidget);
    //	Clamp to the row height so a large accessibility theme can never overflow the row.
    cbWidth = qMin(cbWidth, itemRect.height());
    cbHeight = qMin(cbHeight, itemRect.height());
  }

  QRect checkBoxRect(0, 0, 0, 0);

  QString itemType = data(0, TypeRole).toString();
  if(itemType == "Group")
    checkBoxRect = QRect(itemRect.left(), itemRect.top() + ((itemRect.height() - cbHeight) / 2), 0, 0);
  else if(itemType == "User")
    //	Top-aligned to sit on the same line as the status icon (top + padding), not centered.
    checkBoxRect = QRect(itemRect.left(), itemRect.top() + qBound(0, itemRect.height() - cbHeight, 2), 0, 0);

  if(treeWidget && treeWidget->checkable()) {
    checkBoxRect.setSize(QSize(cbWidth, cbHeight));
    checkBoxRect.moveLeft(checkBoxRect.left() + 3);
  }

  return checkBoxRect;
}

void lmcUserTreeWidgetGroupItem::addChild(QTreeWidgetItem* child) {
  lmcUserTreeWidget* treeWidget = static_cast<lmcUserTreeWidget*>(this->treeWidget());
  child->setSizeHint(0, QSize(0, itemViewHeight[treeWidget->view()]));
  QTreeWidgetItem::addChild(child);
}

bool lmcUserTreeWidgetUserItem::operator<(const QTreeWidgetItem& other) const {
  int column = treeWidget()->sortColumn();
  if(column == 0) {
    //	sort based on status and user name
    if(data(column, StatusRole).toInt() < other.data(column, StatusRole).toInt())
      return true;
    else if(data(column, StatusRole).toInt() > other.data(column, StatusRole).toInt())
      return false;
    else
      return text(column).toLower() < other.text(column).toLower();
  }

  return text(column).toLower() < other.text(column).toLower();
}

void lmcUserTreeWidgetDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  painter->save();

  const QPalette& palette = option.palette;
  bool isEnabled = option.state.testFlag(QStyle::State_Enabled);
  bool isActive = option.state.testFlag(QStyle::State_Active);
  QPalette::ColorGroup group = !isEnabled ? QPalette::Disabled : (isActive ? QPalette::Active : QPalette::Inactive);
  QRect itemRect = option.rect;
  lmcUserTreeWidgetItem* pItem = static_cast<lmcUserTreeWidgetItem*>(index.internalPointer());
  if(!pItem) {
    painter->restore();
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }
  lmcUserTreeWidget* pTreeWidget = static_cast<lmcUserTreeWidget*>(pItem->treeWidget());
  if(!pTreeWidget) {
    painter->restore();
    QStyledItemDelegate::paint(painter, option, index);
    return;
  }
  QString type = pItem->data(0, TypeRole).toString();
  QString name = pItem->data(0, Qt::DisplayRole).toString();
  bool isSelected = option.state.testFlag(QStyle::State_Selected);
  const QWidget* widget = option.widget;
  QStyle* style = widget ? widget->style() : QApplication::style();

  int padding = 2;

  //	Native background (selection, hover, alternate, disabled) via style.
  QStyleOptionViewItem backgroundOpt(option);
  backgroundOpt.palette.setCurrentColorGroup(group);
  backgroundOpt.text = QString();
  backgroundOpt.icon = QIcon();
  if(type == "Group")
    backgroundOpt.features.setFlag(QStyleOptionViewItem::Alternate, false);
  if(type != "User" && type != "Group")
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &backgroundOpt, painter, widget);

  if(type == "Group") {
    //	Section header (MANARA): no box, small muted bold label; a soft rounded
    //	fill only when selected.
    const bool darkGroup = palette.color(group, QPalette::Window).lightness() < 128;
    if(isSelected) {
      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setPen(Qt::NoPen);
      painter->setBrush(darkGroup ? QColor(0x16, 0x25, 0x56) : QColor(0xEA, 0xF1, 0xFD));
      painter->drawRoundedRect(QRectF(itemRect.adjusted(6, 2, -6, -2)), 10, 10);
    }

    //	Draw checkbox if needed (native indicator)
    QRect checkBoxRect = pItem->checkBoxRect(itemRect);
    if(pTreeWidget->checkable())
      drawCheckBox(painter, option, group, checkBoxRect, pItem->checkState(0));

    //	Draw the expand/collapse arrow via style. Qt has no dedicated branch-width metric
    //	(SE_TreeViewDisclosureItem returns the full item rect), so use the DPI-aware
    //	indicator width which matches the checkbox alignment.
    int branchWidth = style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, widget);
    if(branchWidth <= 0)
      branchWidth = 12;
    QRect branchRect(checkBoxRect.right() + 4, itemRect.top(), branchWidth, itemRect.height());
    {
      //  MANARA: thin chevron instead of the style's filled triangle.
      painter->setRenderHint(QPainter::Antialiasing, true);
      painter->setPen(QPen(darkGroup ? QColor(0xA3, 0xB1, 0xD6) : QColor(0x5B, 0x67, 0x85), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter->setBrush(Qt::NoBrush);
      const QPointF c = QRectF(branchRect).center();
      QPainterPath chevron;
      if(pItem->isExpanded()) {
        chevron.moveTo(c.x() - 3.5, c.y() - 1.5);
        chevron.lineTo(c.x(), c.y() + 2.0);
        chevron.lineTo(c.x() + 3.5, c.y() - 1.5);
      } else if(option.direction == Qt::RightToLeft) {
        chevron.moveTo(c.x() + 1.5, c.y() - 3.5);
        chevron.lineTo(c.x() - 2.0, c.y());
        chevron.lineTo(c.x() + 1.5, c.y() + 3.5);
      } else {
        chevron.moveTo(c.x() - 1.5, c.y() - 3.5);
        chevron.lineTo(c.x() + 2.0, c.y());
        chevron.lineTo(c.x() - 1.5, c.y() + 3.5);
      }
      painter->drawPath(chevron);
    }

    //	Draw the text
    QFont font = painter->font();
    font.setBold(true);
    painter->setFont(font);
    int textFlags = Qt::AlignLeft | Qt::AlignVCenter;
    //	Leave a padding of 5px on left and right; clamp so a bad metric can never push the name out.
    int leftPad = branchRect.right() + 4 - itemRect.left();
    int maxPad = itemRect.width() - 5 - 24;
    if(maxPad < padding)
      maxPad = padding;
    leftPad = qBound(padding, leftPad, maxPad);
    QRect textRect = itemRect.adjusted(leftPad, padding, -5, -padding);
    painter->setPen(QPen(darkGroup ? QColor(0xA3, 0xB1, 0xD6) : QColor(0x5B, 0x67, 0x85)));
    QString text = painter->fontMetrics().elidedText(name, Qt::ElideRight, textRect.width());
    painter->drawText(textRect, textFlags, text);
  } else if(type == "User") {
    //	MANARA card: rounded row, circular avatar with a status dot, name and note.
    const bool detailed = pTreeWidget->view() == ULV_Detailed;
    const bool dark = palette.color(group, QPalette::Window).lightness() < 128;
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const int statusIndex = pItem->data(0, StatusRole).toInt();
    const bool offline = (statusIndex == 5);
    const QColor cText = palette.color(group, QPalette::WindowText);
    const QColor cMuted = dark ? QColor(0xA3, 0xB1, 0xD6) : QColor(0x5B, 0x67, 0x85);
    const QColor cSelBg = dark ? QColor(0x16, 0x25, 0x56) : QColor(0xEA, 0xF1, 0xFD);
    const QColor cSelBorder = dark ? QColor(0x24, 0x31, 0x63) : QColor(0xCF, 0xE0, 0xFA);
    const QColor cHover = dark ? QColor(0x12, 0x1E, 0x47) : QColor(0xF1, 0xF5, 0xFC);
    const QColor cPanel = palette.color(group, QPalette::Base);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRect card = itemRect.adjusted(6, 2, -6, -2);
    if(isSelected || hovered) {
      painter->setPen(isSelected ? QPen(cSelBorder, 1) : QPen(Qt::NoPen));
      painter->setBrush(isSelected ? cSelBg : cHover);
      painter->drawRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
    }
    if(offline)
      painter->setOpacity(0.65);

    //	Optional checkbox (user selection dialogs) keeps its native look.
    QRect checkBoxRect = pItem->checkBoxRect(itemRect);
    if(pTreeWidget->checkable())
      drawCheckBox(painter, option, group, checkBoxRect, pItem->checkState(0));

    const int avatarSize = detailed ? 40 : 24;
    int x = card.left() + 8;
    if(pTreeWidget->checkable())
      x = checkBoxRect.right() + 8;
    QRect avatarRect(x, card.center().y() - avatarSize / 2 + 1, avatarSize, avatarSize);
    avatarRect = QStyle::visualRect(option.direction, card, avatarRect);

    //	Avatar clipped to a circle; users without a picture get a brand-coloured initial.
    {
      painter->save();
      QPainterPath clip;
      clip.addEllipse(avatarRect);
      painter->setClipPath(clip);
      QVariant avatar = pItem->data(0, AvatarRole);
      if(!avatar.isNull()) {
        QPixmap pm = avatar.value<QIcon>().pixmap(QSize(avatarSize, avatarSize));
        painter->drawPixmap(avatarRect, pm);
      } else {
        painter->fillRect(avatarRect, QColor(0x1E, 0x4F, 0xC2));
        painter->setPen(Qt::white);
        QFont f = painter->font();
        f.setBold(true);
        f.setPixelSize(avatarSize / 2);
        painter->setFont(f);
        painter->drawText(avatarRect, Qt::AlignCenter, name.left(1).toUpper());
      }
      painter->restore();
    }

    //	Status dot on the avatar corner, ringed with the panel colour.
    {
      QPixmap statusImage = pItem->icon(0).pixmap(QSize(16, 16));
      const int dot = detailed ? 13 : 10;
      QRect dotRect(0, 0, dot, dot);
      dotRect.moveBottomRight(QPoint(avatarRect.right() + (detailed ? 2 : 1), avatarRect.bottom() + (detailed ? 2 : 1)));
      if(option.direction == Qt::RightToLeft)
        dotRect.moveBottomLeft(QPoint(avatarRect.left() - (detailed ? 2 : 1), avatarRect.bottom() + (detailed ? 2 : 1)));
      painter->setPen(Qt::NoPen);
      painter->setBrush(isSelected ? cSelBg : (hovered ? cHover : cPanel));
      painter->drawEllipse(dotRect.adjusted(-2, -2, 2, 2));
      if(!statusImage.isNull())
        painter->drawPixmap(dotRect, statusImage);
    }

    //	Text block beside the avatar.
    const int textGap = 12;
    QRect textRect;
    if(option.direction == Qt::RightToLeft)
      textRect = QRect(card.left() + 8, card.top(), avatarRect.left() - textGap - card.left() - 8, card.height());
    else
      textRect = QRect(avatarRect.right() + textGap, card.top(), card.right() - avatarRect.right() - textGap - 8, card.height());
    const int align = (option.direction == Qt::RightToLeft ? Qt::AlignRight : Qt::AlignLeft);

    //	Unread badge at the far end of the card.
    const int unread = pItem->data(0, UnreadRole).toInt();
    if(unread > 0) {
      const QString label = unread > 99 ? QStringLiteral("99+") : QString::number(unread);
      const int bw = qMax(22, painter->fontMetrics().horizontalAdvance(label) + 14);
      QRect badge(0, 0, bw, 20);
      badge.moveCenter(card.center());
      if(option.direction == Qt::RightToLeft) {
        badge.moveLeft(card.left() + 10);
        textRect.setLeft(badge.right() + 8);
      } else {
        badge.moveRight(card.right() - 10);
        textRect.setRight(badge.left() - 8);
      }
      painter->save();
      painter->setOpacity(1.0);
      painter->setPen(Qt::NoPen);
      painter->setBrush(QColor(0xE8, 0x59, 0x0C));
      painter->drawRoundedRect(badge, 10, 10);
      QFont bf = painter->font();
      bf.setBold(true);
      bf.setPointSizeF(qMax(7.0, bf.pointSizeF() - 1.0));
      painter->setFont(bf);
      painter->setPen(Qt::white);
      painter->drawText(badge, Qt::AlignCenter, label);
      painter->restore();
    }

    QFont nameFont = painter->font();
    nameFont.setBold(true);
    painter->setFont(nameFont);
    painter->setPen(cText);
    QString note = detailed ? pItem->data(0, SubtextRole).toString().trimmed() : QString();
    if(note.isEmpty()) {
      painter->drawText(textRect, align | Qt::AlignVCenter, painter->fontMetrics().elidedText(name, Qt::ElideRight, textRect.width()));
    } else {
      QRect top(textRect.left(), textRect.top() + 6, textRect.width(), textRect.height() / 2 - 4);
      QRect bottom(textRect.left(), textRect.center().y() + 1, textRect.width(), textRect.height() / 2 - 6);
      painter->drawText(top, align | Qt::AlignBottom, painter->fontMetrics().elidedText(name, Qt::ElideRight, top.width()));
      QFont noteFont = painter->font();
      noteFont.setBold(false);
      noteFont.setPointSizeF(qMax(7.0, noteFont.pointSizeF() - 1.0));
      painter->setFont(noteFont);
      painter->setPen(cMuted);
      painter->drawText(bottom, align | Qt::AlignTop, painter->fontMetrics().elidedText(note, Qt::ElideRight, bottom.width()));
    }
  }

  //  Rows draw their own selection; the native focus frame would box them in.
  if(false && option.state.testFlag(QStyle::State_HasFocus)) {
    QStyleOptionFocusRect focusOpt;
    focusOpt.rect = itemRect;
    focusOpt.state = option.state;
    focusOpt.palette = palette;
    focusOpt.palette.setCurrentColorGroup(group);
    focusOpt.backgroundColor = palette.color(group, isSelected ? QPalette::Highlight : QPalette::Window);
    style->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOpt, painter, widget);
  }

  painter->restore();
}

void lmcUserTreeWidgetDelegate::drawCheckBox(QPainter* painter, const QStyleOptionViewItem& option,
                                             QPalette::ColorGroup group, const QRect& checkBoxRect,
                                             Qt::CheckState checkState) const {
  QStyleOptionButton checkOpt;
  checkOpt.rect = checkBoxRect;
  checkOpt.palette = option.palette;
  checkOpt.palette.setCurrentColorGroup(group);
  checkOpt.direction = option.direction;
  checkOpt.fontMetrics = option.fontMetrics;
  checkOpt.styleObject = option.styleObject;
  checkOpt.state = QStyle::State_Enabled;
  if(!option.state.testFlag(QStyle::State_Enabled))
    checkOpt.state = QStyle::State_None;
  checkOpt.state |= (option.state & (QStyle::State_MouseOver | QStyle::State_HasFocus | QStyle::State_Selected |
                                     QStyle::State_Sunken | QStyle::State_Active));
  if(checkState == Qt::Checked)
    checkOpt.state |= QStyle::State_On;
  else if(checkState == Qt::PartiallyChecked)
    checkOpt.state |= QStyle::State_NoChange;
  else
    checkOpt.state |= QStyle::State_Off;
  const QWidget* widget = option.widget;
  QStyle* style = widget ? widget->style() : QApplication::style();
  style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &checkOpt, painter, widget);
}

lmcUserTreeWidget::lmcUserTreeWidget(QWidget* parent)
    : QTreeWidget(parent) {
  itemDelegate = new lmcUserTreeWidgetDelegate();
  setItemDelegate(itemDelegate);
  setAlternatingRowColors(true);

  isCheckable = false;
  viewType = ULV_Detailed;
}

bool lmcUserTreeWidget::checkable(void) {
  return isCheckable;
}

void lmcUserTreeWidget::setCheckable(bool enable) {
  isCheckable = enable;
}

UserListView lmcUserTreeWidget::view(void) {
  return viewType;
}

void lmcUserTreeWidget::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
  QStyleOptionViewItem opt(option);
  opt.rect = visualRect(index);
  //  QTreeView computes these per row internally; rebuild them for the delegate.
  if(selectionModel() && selectionModel()->isSelected(index))
    opt.state |= QStyle::State_Selected;
  if(index == currentIndex() && hasFocus())
    opt.state |= QStyle::State_HasFocus;
  if(window() && window()->isActiveWindow())
    opt.state |= QStyle::State_Active;
  if(underMouse() && indexAt(viewport()->mapFromGlobal(QCursor::pos())) == index)
    opt.state |= QStyle::State_MouseOver;
  QAbstractItemDelegate* d = QTreeWidget::itemDelegate();
  if(d)
    d->paint(painter, opt, index);
  else
    QTreeWidget::drawRow(painter, option, index);
}

void lmcUserTreeWidget::setView(UserListView view) {
  viewType = view;

  //	Set the item heights for the selected view type
  for(int index = 0; index < topLevelItemCount(); index++) {
    QTreeWidgetItem* item = topLevelItem(index);
    for(int childIndex = 0; childIndex < item->childCount(); childIndex++)
      item->child(childIndex)->setSizeHint(0, QSize(0, itemViewHeight[viewType]));
  }
}

void lmcUserTreeWidget::mousePressEvent(QMouseEvent* event) {
  if(event->button() == Qt::LeftButton) {
    QTreeWidgetItem* item = itemAt(event->position().toPoint());

    dragGroup = false;
    dragUser = false;
    dragItem = NULL;
    parentId = QString();
    expanded = false;

    if(item) {
      dragItem = item;
      if(dynamic_cast<lmcUserTreeWidgetGroupItem*>(item)) {
        dragGroup = true;
        expanded = dragItem->isExpanded();
      } else if(dynamic_cast<lmcUserTreeWidgetUserItem*>(item)) {
        dragUser = true;
        parentId = dragItem->parent()->data(0, IdRole).toString();
      }
    }
  }

  QTreeWidget::mousePressEvent(event);
}

void lmcUserTreeWidget::dragMoveEvent(QDragMoveEvent* event) {
  QTreeWidget::dragMoveEvent(event);

  QTreeWidgetItem* item = itemAt(event->position().toPoint());
  bool accept = false;

  if(dragUser) {
    if(item && dynamic_cast<lmcUserTreeWidgetGroupItem*>(item) && visualItemRect(item).contains(event->position().toPoint(), true))
      accept = true;
  } else if(dragGroup) {
    if(!item || (dynamic_cast<lmcUserTreeWidgetGroupItem*>(item) && !visualItemRect(item).contains(event->position().toPoint(), true)))
      accept = true;
  }

  accept ? event->accept() : event->ignore();
}

void lmcUserTreeWidget::dropEvent(QDropEvent* event) {
  QTreeWidget::dropEvent(event);

  if(dragUser) {
    if(!dragItem->parent()) {
      //	user item dragged to group level. revert
      for(int index = 0; index < topLevelItemCount(); index++) {
        QTreeWidgetItem* parentItem = topLevelItem(index);
        if(parentItem->data(0, IdRole).toString().compare(parentId) == 0) {
          takeTopLevelItem(indexOfTopLevelItem(dragItem));
          parentItem->addChild(dragItem);
          return;
        }
      }
    }
  } else if(dragGroup) {
    dragItem->setExpanded(expanded);
  }

  if(dragItem)
    emit itemDragDropped(dragItem);
}

void lmcUserTreeWidget::contextMenuEvent(QContextMenuEvent* event) {
  QTreeWidget::contextMenuEvent(event);

  QTreeWidgetItem* item = itemAt(event->pos());
  QPoint pos = event->globalPos();
  if(item && event->reason() != QContextMenuEvent::Mouse) {
    QRect itemRect = visualItemRect(item);
    pos = itemRect.bottomLeft();
    pos.ry() += itemRect.height();
    pos = mapToGlobal(pos);
  }

  emit itemContextMenu(item, pos);
}

void lmcUserTreeWidget::mouseReleaseEvent(QMouseEvent* event) {
  QTreeWidget::mouseReleaseEvent(event);

  QPoint pos = event->position().toPoint();
  lmcUserTreeWidgetItem* item = static_cast<lmcUserTreeWidgetItem*>(itemAt(pos));
  if(item && checkable() && item->checkBoxRect(visualItemRect(item)).contains(pos)) {
    // toggle checkstate
    if(item->checkState(0) == Qt::Checked)
      item->setCheckState(0, Qt::Unchecked);
    else
      item->setCheckState(0, Qt::Checked);
  }
}

void lmcUserTreeWidget::keyPressEvent(QKeyEvent* event) {
  QTreeWidget::keyPressEvent(event);

  if(event->key() == Qt::Key_Space && selectedItems().count() > 0) {
    lmcUserTreeWidgetItem* item = static_cast<lmcUserTreeWidgetItem*>(selectedItems().at(0));
    if(item && checkable()) {
      // toggle checkstate
      if(item->checkState(0) == Qt::Checked)
        item->setCheckState(0, Qt::Unchecked);
      else
        item->setCheckState(0, Qt::Checked);
    }
  }
}
