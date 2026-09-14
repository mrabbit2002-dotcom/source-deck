#include "source-deck-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QSignalBlocker>
#include <QStandardPaths>

static QString currentSceneName()
{
  obs_source_t *scene = obs_frontend_get_current_scene();
  if (!scene)
    return {};

  const QString name = QString::fromUtf8(obs_source_get_name(scene));
  obs_source_release(scene);
  return name;
}

SourceDeckDock::SourceDeckDock(Mode mode, QWidget *parent)
  : QWidget(parent), mode(mode)
{
  grid = new QGridLayout(this);
  grid->setContentsMargins(8, 8, 8, 8);
  grid->setSpacing(6);
  setLayout(grid);

  loadLayout();
  refreshGrid();

  connect(&timer, &QTimer::timeout, this, [this]() { syncStates(); });
  timer.start(250);
}

QString SourceDeckDock::settingsPath() const
{
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  const QString fileName = mode == Mode::Scenes ? "scene-deck-layout.json" : "source-deck-layout.json";
  return dir + "/" + fileName;
}

void SourceDeckDock::clearDeck()
{
  while (QLayoutItem *child = grid->takeAt(0)) {
    if (child->widget())
      child->widget()->deleteLater();
    delete child;
  }

  editButton = nullptr;
  addButton = nullptr;
  for (auto &entry : entries)
    entry.button = nullptr;
}

void SourceDeckDock::buildControls()
{
  editButton = new QPushButton(editMode ? "Done" : "Edit", this);
  editButton->setMinimumHeight(38);
  connect(editButton, &QPushButton::clicked, this, [this]() {
    editMode = !editMode;
    if (!editMode)
      saveLayout();
    refreshGrid();
  });
  grid->addWidget(editButton, 0, 0);

  if (editMode) {
    addButton = new QPushButton(mode == Mode::Scenes ? "+ Scene" : "+ Source", this);
    addButton->setMinimumHeight(38);
    connect(addButton, &QPushButton::clicked, this, [this]() { chooseAndAdd(); });
    grid->addWidget(addButton, 0, 1);
  }
}

QStringList SourceDeckDock::availableItems() const
{
  QStringList result;

  if (mode == Mode::Scenes) {
    obs_frontend_source_list list{};
    obs_frontend_get_scenes(&list);
    for (size_t i = 0; i < list.sources.num; ++i)
      result << QString::fromUtf8(obs_source_get_name(list.sources.array[i]));
    obs_frontend_source_list_free(&list);
  } else {
    obs_source_t *sceneSource = obs_frontend_get_current_scene();
    if (!sceneSource)
      return result;

    obs_scene_t *scene = obs_scene_from_source(sceneSource);
    if (scene) {
      obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *data) {
        auto *out = static_cast<QStringList *>(data);
        obs_source_t *source = obs_sceneitem_get_source(item);
        if (source)
          out->append(QString::fromUtf8(obs_source_get_name(source)));
        return true;
      }, &result);
    }
    obs_source_release(sceneSource);
  }

  result.removeDuplicates();
  result.sort(Qt::CaseInsensitive);

  for (const auto &entry : entries)
    result.removeAll(entry.name);

  return result;
}

void SourceDeckDock::chooseAndAdd()
{
  const QStringList choices = availableItems();
  if (choices.isEmpty())
    return;

  bool ok = false;
  const QString title = mode == Mode::Scenes ? "Add Scene" : "Add Source";
  const QString label = mode == Mode::Scenes ? "Scene:" : "Source:";
  const QString selected = QInputDialog::getItem(this, title, label, choices, 0, false, &ok);
  if (!ok || selected.isEmpty())
    return;

  entries.push_back({nullptr, selected});
  saveLayout();
  refreshGrid();
}

void SourceDeckDock::addDeckButton(const QString &name, int index)
{
  auto *button = new QPushButton(name, this);
  button->setMinimumSize(105, 64);
  button->setCheckable(mode == Mode::Sources);

  if (mode == Mode::Scenes) {
    button->setStyleSheet(
      "QPushButton{font-weight:600;padding:6px;border:2px solid #5b7cfa;}"
    );
  } else {
    button->setStyleSheet(
      "QPushButton{font-weight:600;padding:6px;}"
      "QPushButton:checked{background:#3a9d5d;color:white;}"
    );
  }

  connect(button, &QPushButton::clicked, this, [this, index]() {
    if (!editMode)
      activateEntry(index);
  });

  if (editMode) {
    button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(button, &QPushButton::customContextMenuRequested, this,
            [this, index, button](const QPoint &pos) {
      QMenu menu;
      QAction *left = menu.addAction("Move Left");
      QAction *right = menu.addAction("Move Right");
      menu.addSeparator();
      QAction *remove = menu.addAction("Remove");
      QAction *picked = menu.exec(button->mapToGlobal(pos));

      if (picked == left && index > 0) {
        moveEntry(index, index - 1);
      } else if (picked == right && index + 1 < entries.size()) {
        moveEntry(index, index + 1);
      } else if (picked == remove) {
        entries.removeAt(index);
        saveLayout();
        refreshGrid();
      }
    });
  }

  entries[index].button = button;
}

void SourceDeckDock::activateEntry(int index)
{
  if (index < 0 || index >= entries.size())
    return;

  const QString name = entries[index].name;

  if (mode == Mode::Scenes) {
    obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
    if (scene) {
      obs_frontend_set_current_scene(scene);
      obs_source_release(scene);
    }
    return;
  }

  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  if (!sceneSource)
    return;

  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (scene) {
    obs_sceneitem_t *item = obs_scene_find_source(scene, name.toUtf8().constData());
    if (item)
      obs_sceneitem_set_visible(item, !obs_sceneitem_visible(item));
  }

  obs_source_release(sceneSource);
}

void SourceDeckDock::moveEntry(int from, int to)
{
  if (from < 0 || to < 0 || from >= entries.size() || to >= entries.size())
    return;

  entries.move(from, to);
  saveLayout();
  refreshGrid();
}

void SourceDeckDock::refreshGrid()
{
  clearDeck();
  buildControls();

  constexpr int columns = 3;
  for (int i = 0; i < entries.size(); ++i) {
    addDeckButton(entries[i].name, i);
    grid->addWidget(entries[i].button, 1 + (i / columns), i % columns);
  }

  syncStates();
}

void SourceDeckDock::rebuild()
{
  refreshGrid();
}

void SourceDeckDock::syncStates()
{
  if (mode == Mode::Scenes) {
    const QString activeScene = currentSceneName();
    for (auto &entry : entries) {
      if (!entry.button)
        continue;

      const bool active = entry.name == activeScene;
      entry.button->setStyleSheet(
        active
          ? "QPushButton{font-weight:700;padding:6px;background:#5b7cfa;color:white;border:2px solid #5b7cfa;}"
          : "QPushButton{font-weight:600;padding:6px;border:2px solid #5b7cfa;}"
      );
    }
    return;
  }

  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource) : nullptr;

  for (auto &entry : entries) {
    if (!entry.button)
      continue;

    obs_sceneitem_t *item = scene ? obs_scene_find_source(scene, entry.name.toUtf8().constData()) : nullptr;
    entry.button->setEnabled(item != nullptr || editMode);

    QSignalBlocker blocker(entry.button);
    entry.button->setChecked(item && obs_sceneitem_visible(item));
  }

  if (sceneSource)
    obs_source_release(sceneSource);
}

void SourceDeckDock::saveLayout()
{
  QJsonArray array;
  for (const auto &entry : entries) {
    QJsonObject obj;
    obj["name"] = entry.name;
    array.append(obj);
  }

  QFile file(settingsPath());
  if (file.open(QIODevice::WriteOnly))
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

void SourceDeckDock::loadLayout()
{
  QFile file(settingsPath());
  if (!file.open(QIODevice::ReadOnly))
    return;

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isArray())
    return;

  for (const auto &value : doc.array()) {
    const QString name = value.toObject()["name"].toString();
    if (!name.isEmpty())
      entries.push_back({nullptr, name});
  }
}
