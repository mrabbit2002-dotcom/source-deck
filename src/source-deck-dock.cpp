#include "source-deck-dock.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QStandardPaths>
#include <QVBoxLayout>

static QString currentSceneName()
{
  obs_source_t *scene = obs_frontend_get_current_scene();
  if (!scene) return {};
  const QString name = QString::fromUtf8(obs_source_get_name(scene));
  obs_source_release(scene);
  return name;
}

SourceDeckDock::SourceDeckDock(QWidget *parent) : QWidget(parent)
{
  grid = new QGridLayout(this);
  grid->setContentsMargins(8, 8, 8, 8);
  grid->setSpacing(6);
  setLayout(grid);
  loadLayout();
  rebuild();
  connect(&timer, &QTimer::timeout, this, [this]() { syncStates(); });
  timer.start(250);
}

QString SourceDeckDock::settingsPath() const
{
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  return dir + "/source-deck-layout.json";
}

void SourceDeckDock::clearDeck()
{
  while (QLayoutItem *child = grid->takeAt(0)) {
    if (child->widget()) child->widget()->deleteLater();
    delete child;
  }
  editButton = nullptr;
  addSceneButton = nullptr;
  addSourceButton = nullptr;
}

void SourceDeckDock::buildControls()
{
  editButton = new QPushButton(editMode ? "Done" : "Edit", this);
  editButton->setMinimumHeight(38);
  connect(editButton, &QPushButton::clicked, this, [this]() {
    editMode = !editMode;
    if (!editMode) saveLayout();
    refreshGrid();
  });
  grid->addWidget(editButton, 0, 0);

  if (editMode) {
    addSceneButton = new QPushButton("+ Scene", this);
    addSourceButton = new QPushButton("+ Source", this);
    connect(addSceneButton, &QPushButton::clicked, this, [this]() { chooseAndAdd(DeckType::Scene); });
    connect(addSourceButton, &QPushButton::clicked, this, [this]() { chooseAndAdd(DeckType::Source); });
    grid->addWidget(addSceneButton, 0, 1);
    grid->addWidget(addSourceButton, 0, 2);
  }
}

QStringList SourceDeckDock::availableScenes() const
{
  QStringList result;
  obs_frontend_source_list list{};
  obs_frontend_get_scenes(&list);
  for (size_t i = 0; i < list.sources.num; ++i)
    result << QString::fromUtf8(obs_source_get_name(list.sources.array[i]));
  obs_frontend_source_list_free(&list);
  return result;
}

QStringList SourceDeckDock::availableSources() const
{
  QStringList result;
  obs_source_enum_all([](void *data, obs_source_t *source) {
    auto *out = static_cast<QStringList *>(data);
    const uint32_t flags = obs_source_get_output_flags(source);
    if (!(flags & OBS_SOURCE_CAP_DISABLED))
      out->append(QString::fromUtf8(obs_source_get_name(source)));
    return true;
  }, &result);
  result.removeDuplicates();
  result.sort(Qt::CaseInsensitive);
  return result;
}

void SourceDeckDock::chooseAndAdd(DeckType type)
{
  const QStringList choices = type == DeckType::Scene ? availableScenes() : availableSources();
  if (choices.isEmpty()) return;

  QDialog dialog(this);
  dialog.setWindowTitle(type == DeckType::Scene ? "Add Scene" : "Add Source");
  auto *layout = new QVBoxLayout(&dialog);
  auto *combo = new QComboBox(&dialog);
  combo->addItems(choices);
  layout->addWidget(combo);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted) return;

  entries.push_back({nullptr, type, combo->currentText()});
  saveLayout();
  refreshGrid();
}

void SourceDeckDock::addDeckButton(DeckType type, const QString &name, int index)
{
  auto *button = new QPushButton(name, this);
  button->setMinimumSize(105, 64);
  button->setCheckable(type == DeckType::Source);
  button->setProperty("deckType", type == DeckType::Scene ? "scene" : "source");
  button->setStyleSheet(
    "QPushButton{font-weight:600;padding:6px;}"
    "QPushButton[deckType=scene]{border:2px solid #5b7cfa;}"
    "QPushButton[deckType=source]:checked{background:#3a9d5d;color:white;}"
  );
  connect(button, &QPushButton::clicked, this, [this, index]() {
    if (!editMode) activateEntry(index);
  });

  if (editMode) {
    button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(button, &QPushButton::customContextMenuRequested, this, [this, index, button](const QPoint &pos) {
      QMenu menu;
      QAction *left = menu.addAction("Move Left");
      QAction *right = menu.addAction("Move Right");
      menu.addSeparator();
      QAction *remove = menu.addAction("Remove");
      QAction *picked = menu.exec(button->mapToGlobal(pos));
      if (picked == left && index > 0) moveEntry(index, index - 1);
      else if (picked == right && index + 1 < entries.size()) moveEntry(index, index + 1);
      else if (picked == remove) {
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
  if (index < 0 || index >= entries.size()) return;
  Entry &entry = entries[index];
  if (entry.type == DeckType::Scene) {
    obs_source_t *scene = obs_get_source_by_name(entry.name.toUtf8().constData());
    if (scene) {
      obs_frontend_set_current_scene(scene);
      obs_source_release(scene);
    }
    return;
  }

  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  if (!sceneSource) return;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (scene) {
    obs_sceneitem_t *item = obs_scene_find_source(scene, entry.name.toUtf8().constData());
    if (item) obs_sceneitem_set_visible(item, !obs_sceneitem_visible(item));
  }
  obs_source_release(sceneSource);
}

void SourceDeckDock::moveEntry(int from, int to)
{
  if (from < 0 || to < 0 || from >= entries.size() || to >= entries.size()) return;
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
    addDeckButton(entries[i].type, entries[i].name, i);
    const int cell = i + columns;
    grid->addWidget(entries[i].button, cell / columns, cell % columns);
  }
  syncStates();
}

void SourceDeckDock::rebuild()
{
  currentSceneName = currentSceneName();
  refreshGrid();
}

void SourceDeckDock::syncStates()
{
  const QString sceneNow = ::currentSceneName();
  currentSceneName = sceneNow;
  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource) : nullptr;

  for (auto &entry : entries) {
    if (!entry.button) continue;
    if (entry.type == DeckType::Scene) {
      entry.button->setStyleSheet(
        entry.name == sceneNow
          ? "QPushButton{font-weight:700;padding:6px;background:#5b7cfa;color:white;border:2px solid #5b7cfa;}"
          : "QPushButton{font-weight:600;padding:6px;border:2px solid #5b7cfa;}"
      );
    } else if (scene) {
      obs_sceneitem_t *item = obs_scene_find_source(scene, entry.name.toUtf8().constData());
      entry.button->setEnabled(item != nullptr || editMode);
      if (item) entry.button->setChecked(obs_sceneitem_visible(item));
      else entry.button->setChecked(false);
    }
  }
  if (sceneSource) obs_source_release(sceneSource);
}

void SourceDeckDock::saveLayout()
{
  QJsonArray array;
  for (const auto &entry : entries) {
    QJsonObject obj;
    obj["type"] = entry.type == DeckType::Scene ? "scene" : "source";
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
  if (!file.open(QIODevice::ReadOnly)) return;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isArray()) return;
  for (const auto &value : doc.array()) {
    const QJsonObject obj = value.toObject();
    const QString name = obj["name"].toString();
    if (name.isEmpty()) continue;
    const DeckType type = obj["type"].toString() == "scene" ? DeckType::Scene : DeckType::Source;
    entries.push_back({nullptr, type, name});
  }
}
