// MIT License
//
// Copyright (c) 2017-2018 Artur Wyszyński, aljen at hitomi dot pl
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#ifndef UI_PACKAGE_VIEW_H
#define UI_PACKAGE_VIEW_H

#include <QGraphicsView>
#include <QHash>
#include <QTimer>

class QTableWidget;

namespace spaghetti {

class Package;
class Node;
class LinkItem;

class PackageView final : public QGraphicsView {
  Q_OBJECT

 public:
  using Nodes = QHash<size_t, Node *>;

  explicit PackageView(QTableWidget *const a_properties, Package *const a_package = nullptr);
  ~PackageView() override;

  void open();
  void save();

  void dragEnterEvent(QDragEnterEvent *a_event) override;
  void dragLeaveEvent(QDragLeaveEvent *a_event) override;
  void dragMoveEvent(QDragMoveEvent *a_event) override;
  void dropEvent(QDropEvent *a_event) override;
  void keyPressEvent(QKeyEvent *a_event) override;
  void keyReleaseEvent(QKeyEvent *a_event) override;
  void wheelEvent(QWheelEvent *a_event) override;
  void drawBackground(QPainter *painter, QRectF const &a_rect) override;

  LinkItem *dragLink() const { return m_dragLink; }
  void setDragLink(LinkItem *a_link) { m_dragLink = a_link; }
  void acceptDragLink() { m_dragLink = nullptr; }
  void cancelDragLink();

  Package const *package() const { return m_package; }
  Package *package() { return m_package; }

  bool canClose();
  void center();
  bool snapToGrid() const { return m_snapToGrid; }

  void setFilename(QString const a_filename) { m_filename = a_filename; }
  QString filename() const { return m_filename; }

  void showProperties();

  void deleteElement();

  Nodes &nodes() { return m_nodes; }
  Nodes const &nodes() const { return m_nodes; }

  Node *getNode(size_t const a_id) const { return m_nodes[a_id]; }

  void setSelectedNode(Node *const a_node);

 signals:
  void requestOpenFile(QString const a_filename);

 private:
  void updateGrid(qreal const a_scale);

 private:
  QTableWidget *const m_properties{};
  Package *const m_package{};
  Nodes m_nodes{};
  QGraphicsScene *const m_scene{};
  QTimer m_timer{};
  Node *const m_inputs{};
  Node *const m_outputs{};
  Node *const m_packageNode{};
  Node *m_dragNode{};
  Node *m_selectedNode{};
  LinkItem *m_dragLink{};
  int32_t m_scheduledScalings{};
  enum class GridDensity { eLarge, eSmall } m_gridDensity{};
  QString m_filename{};
  bool m_snapToGrid{};
  bool m_standalone{};
};

} // namespace spaghetti

#endif // UI_PACKAGE_VIEW_H
