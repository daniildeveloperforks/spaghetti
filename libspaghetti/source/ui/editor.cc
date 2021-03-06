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

#include "spaghetti/editor.h"
#include "ui_editor.h"

#include "spaghetti/socket_item.h"
#include "ui/colors.h"
#include "ui/elements_list.h"
#include "ui/link_item.h"

#include <QAction>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <cctype>
#include <fstream>
#include <typeinfo>
#include <vector>

#include "elements/logic/all.h"
#include "spaghetti/node.h"
#include "spaghetti/registry.h"
#include "spaghetti/version.h"
#include "ui/expander_widget.h"
#include "ui/package_view.h"

QString const PACKAGES_DIR{ "../packages" };

namespace spaghetti {

Editor::Editor(QWidget *const a_parent)
  : QMainWindow{ a_parent }
  , m_ui{ new Ui::Editor }
{
  setObjectName("SpaghettiEditor");
  m_ui->setupUi(this);
  m_ui->libraryContainer->removeItem(0);
  m_ui->tabWidget->removeTab(0);

  connect(m_ui->actionNew, &QAction::triggered, this, &Editor::newPackage);
  connect(m_ui->actionOpen, &QAction::triggered, this, &Editor::openPackage);
  connect(m_ui->actionSave, &QAction::triggered, this, &Editor::savePackage);
  connect(m_ui->actionSaveAs, &QAction::triggered, this, &Editor::saveAsPackage);
  connect(m_ui->actionClose, &QAction::triggered, this, &Editor::closePackage);
  connect(m_ui->actionCloseAll, &QAction::triggered, this, &Editor::closeAllPackages);

  connect(m_ui->actionDeleteElement, &QAction::triggered, this, &Editor::deleteElement);

  connect(m_ui->actionShowLibrary, &QAction::triggered, this, &Editor::showLibrary);
  connect(m_ui->actionShowProperties, &QAction::triggered, this, &Editor::showProperties);

  connect(m_ui->actionBuildCommit, &QAction::triggered, this, &Editor::buildCommit);
  connect(m_ui->actionRecentChanges, &QAction::triggered, this, &Editor::recentChanges);
  connect(m_ui->actionAbout, &QAction::triggered, this, &Editor::about);
  connect(m_ui->actionAboutQt, &QAction::triggered, this, &Editor::aboutQt);
  connect(m_ui->tabWidget, &QTabWidget::tabCloseRequested, this, &Editor::tabCloseRequested);
  connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, &Editor::tabChanged);

  QDir packagesDir{ PACKAGES_DIR };
  if (!packagesDir.exists()) packagesDir.mkpath(".");

  populateLibrary();
  newPackage();
}

Editor::~Editor()
{
  closeAllPackages();
  delete m_ui;
}

void Editor::tabCloseRequested(int const a_index)
{
  QTabWidget *const tab{ m_ui->tabWidget };
  QWidget *const widget{ tab->widget(a_index) };
  PackageView *const packageView{ reinterpret_cast<PackageView *>(widget) };

  if (packageView->canClose()) {
    QString const FILENAME = packageView->filename();
    if (!FILENAME.isEmpty()) m_openFiles.remove(FILENAME);
    tab->removeTab(a_index);
    delete packageView;
  }

  int const SIZE{ tab->count() };

  for (int i = 0; i < SIZE; ++i) {
    QWidget *const tempWidget{ tab->widget(i) };
    PackageView *const tempPackageView{ reinterpret_cast<PackageView *>(tempWidget) };
    QString const FILENAME = tempPackageView->filename();
    if (FILENAME.isEmpty()) continue;

    m_openFiles[FILENAME] = i;
  }
}

void Editor::tabChanged(int const a_index)
{
  m_packageViewIndex = a_index;
}

void Editor::populateLibrary()
{
  Registry const &registry{ Registry::get() };
  auto const &SIZE{ registry.size() };

  for (size_t i = 0; i < SIZE; ++i) {
    auto const &info = registry.metaInfoAt(i);
    std::string const path{ info.type };
    std::string category{ path };

    if (auto const it = path.find_first_of('/'); it != std::string::npos) category = path.substr(0, it);
    category[0] = static_cast<char>(std::toupper(category[0]));

    addElement(QString::fromStdString(category), QString::fromStdString(info.name), QString::fromStdString(info.type),
               QString::fromStdString(info.icon));
  }
}

void Editor::addElement(QString a_category, QString a_name, QString a_type, QString a_icon)
{
  ExpanderWidget *const library{ m_ui->libraryContainer };

  ElementsList *list{};

  int const count{ library->count() };
  for (int i = 0; i < count; ++i) {
    QString const text{ library->itemText(i) };
    if (text != a_category) continue;

    list = qobject_cast<ElementsList *>(library->widget(i));
    assert(list);
    break;
  }

  if (list == nullptr) {
    list = new ElementsList{ this };
    library->addItem(list, a_category);
  }

  QListWidgetItem *const item{ new QListWidgetItem{ a_name } };
  item->setData(ElementsList::eMetaDataType, a_type);
  item->setData(ElementsList::eMetaDataName, a_name);
  item->setData(ElementsList::eMetaDataIcon, a_icon);
  item->setIcon(QIcon(a_icon));

  list->addItem(item);
  list->doResize();
  list->sortItems();
}

void Editor::aboutToQuit() {}

void Editor::showEvent(QShowEvent *a_event)
{
  static bool s_firstTime{ true };

  if (s_firstTime) {
    s_firstTime = false;
    auto const tab = m_ui->tabWidget;
    auto const index = tab->currentIndex();
    auto packageView = qobject_cast<PackageView *const>(tab->widget(index));
    packageView->center();
  }

  QMainWindow::showEvent(a_event);
}

PackageView *Editor::packageViewForIndex(int const a_index) const
{
  auto const tabWidget = m_ui->tabWidget;
  auto const index = a_index == -1 ? tabWidget->currentIndex() : a_index;
  auto packageView = qobject_cast<PackageView *const>(tabWidget->widget(index));
  return packageView;
}

int Editor::openPackageViews() const
{
  return m_ui->tabWidget->count();
}

void Editor::newPackage()
{
  auto const packageView = new PackageView{ m_ui->propertiesTable };
  m_packageViewIndex = m_ui->tabWidget->addTab(packageView, "New package");
  m_ui->tabWidget->setCurrentIndex(m_packageViewIndex);
  packageView->setSelectedNode(nullptr);
  packageView->showProperties();

  connect(packageView, &PackageView::requestOpenFile,
          [this](QString const a_filename) { openPackageFile(a_filename); });
}

void Editor::openPackage()
{
  foreach (PackageView *temp, this->findChildren<PackageView *>())
    temp->setUpdatesEnabled(false);

  QString const FILENAME{ QFileDialog::getOpenFileName(this, "Open .package", PACKAGES_DIR, "*.package") };

  foreach (PackageView *temp, this->findChildren<PackageView *>())
    temp->setUpdatesEnabled(true);

  if (FILENAME.isEmpty()) return;

  openPackageFile(FILENAME);
}

void Editor::openPackageFile(QString const a_filename)
{
  auto const FOUND = m_openFiles.constFind(a_filename);

  if (FOUND != m_openFiles.constEnd()) {
    m_packageViewIndex = FOUND.value();
    m_ui->tabWidget->setCurrentIndex(m_packageViewIndex);
    return;
  }

  newPackage();

  auto const packageView = packageViewForIndex(m_packageViewIndex);
  packageView->setFilename(a_filename);

  QDir const PACKAGES{ PACKAGES_DIR };
  m_ui->tabWidget->setTabText(m_packageViewIndex, PACKAGES.relativeFilePath(a_filename));

  packageView->open();
  packageView->setSelectedNode(nullptr);
  packageView->showProperties();

  m_openFiles[a_filename] = m_packageViewIndex;
}

void Editor::savePackage()
{
  savePackageView(false);
}

void Editor::saveAsPackage()
{
  savePackageView(true);
}

void Editor::closePackage()
{
  closePackageView(m_packageViewIndex);
}

void Editor::closeAllPackages()
{
  while (int count = openPackageViews()) closePackageView(count - 1);
}

void Editor::savePackageView(bool const a_saveAs)
{
  assert(m_packageViewIndex >= 0);

  auto const packageView = packageViewForIndex(m_packageViewIndex);

  if (a_saveAs || packageView->filename().isEmpty()) {
    foreach (PackageView *temp, this->findChildren<PackageView *>())
      temp->setUpdatesEnabled(false);

    QString filename{ QFileDialog::getSaveFileName(this, "Save .package", PACKAGES_DIR, "*.package") };

    foreach (PackageView *temp, this->findChildren<PackageView *>())
      temp->setUpdatesEnabled(true);

    if (filename.isEmpty()) return;
    if (!filename.endsWith(".package")) filename += ".package";

    packageView->setFilename(filename);
    QDir const packagesDir{ PACKAGES_DIR };
    m_ui->tabWidget->setTabText(m_packageViewIndex, packagesDir.relativeFilePath(filename));
  }

  packageView->save();
}

void Editor::closePackageView(int const a_index)
{
  auto const packageView = packageViewForIndex(a_index);
  if (packageView->canClose()) m_ui->tabWidget->removeTab(a_index);
  delete packageView;
}

void Editor::deleteElement()
{
  auto const packageView = packageViewForIndex(m_packageViewIndex);
  packageView->deleteElement();
}

void Editor::showLibrary(bool a_checked)
{
  m_ui->library->setVisible(a_checked);
}

void Editor::showProperties(bool a_checked)
{
  m_ui->properties->setVisible(a_checked);
}

void Editor::buildCommit()
{
  QUrl const url{ QString("https://github.com/aljen/spaghetti/tree/%1").arg(version::COMMIT_HASH) };
  QDesktopServices::openUrl(url);
}

void Editor::recentChanges()
{
  QUrl const url{ QString("https://github.com/aljen/spaghetti/compare/%1...master").arg(version::COMMIT_HASH) };
  QDesktopServices::openUrl(url);
}

void Editor::about()
{
  QMessageBox::about(
      this, "About Spaghetti",
      QString("<a href='https://github.com/aljen/spaghetti'>Spaghetti</a> version: %1<br>"
              "<br>"
              "Copyright © 2017-2018 <b>Artur Wyszyński</b><br>"
              "<br>"
              "Build date: <b>%2, %3</b><br>"
              "Git branch: <b>%4</b><br>"
              "Git commit: <b>%5</b><br>"
              "<br>"
              "Used libraries:<br>"
              "<a href='https://github.com/nlohmann/json'>JSON for Modern C++</a> by <b>Niels Lohmann</b><br>"
              "<a href='https://github.com/cameron314/concurrentqueue'>An industrial-strength lock-free queue for "
              "C++</a> by <b>cameron314</b><br>"
              "<a href='https://github.com/greg7mdp/sparsepp'>A fast, memory efficient hash map for C++</a> by "
              "<b>Gregory Popovitch</b><br>"
              "<a href='http://www.boost.org/'>Boost libraries</a><br>")
          .arg(version::STRING)
          .arg(__DATE__)
          .arg(__TIME__)
          .arg(version::BRANCH)
          .arg(version::COMMIT_SHORT_HASH));
}

void Editor::aboutQt()
{
  QMessageBox::aboutQt(this);
}

} // namespace spaghetti
