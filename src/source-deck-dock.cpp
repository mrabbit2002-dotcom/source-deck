#include "source-deck-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QSignalBlocker>
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

SourceDeckDock::SourceDeckDock(Mode mode, QWidget *parent) : QWidget(parent), mode(mode)
{
  buildUi();
  loadLayout();
  rebuildButtons();
  connect(&timer, &QTimer::timeout, this, [this]() { syncStates(); });
  timer.start(250);
}

void SourceDeckDock::buildUi()
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(6);
  auto *toolbar = new QHBoxLayout();
  toolbar->setSpacing(6);
  editButton = new QPushButton("Edit", this);
  editButton->setMinimumHeight(36);
  toolbar->addWidget(editButton);
  addButton = new QPushButton(mode == Mode::Scenes ? "+ Scene" : "+ Source", this);
  addButton->setMinimumHeight(36);
  addButton->setVisible(false);
  toolbar->addWidget(addButton);
  toolbar->addStretch(1);
  root->addLayout(toolbar);
  canvas = new QWidget(this);
  canvas->setMinimumSize(360, 320);
  canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  canvas->setStyleSheet("QWidget{background:rgba(127,127,127,0.06);border:1px solid rgba(127,127,127,0.20);}");
  root->addWidget(canvas, 1);
  connect(editButton, &QPushButton::clicked, this, [this]() {
    editMode = !editMode;
    editButton->setText(editMode ? "Done" : "Edit");
    addButton->setVisible(editMode);
    updateButtonAppearance();
    if (!editMode) saveLayout();
  });
  connect(addButton, &QPushButton::clicked, this, [this]() { chooseAndAdd(); });
}

QString SourceDeckDock::settingsPath() const
{
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  return dir + "/" + (mode == Mode::Scenes ? "scene-deck-layout.json" : "source-deck-layout.json");
}

QStringList SourceDeckDock::availableItems() const
{
  QStringList result;
  if (mode == Mode::Scenes) {
    obs_frontend_source_list list{};
    obs_frontend_get_scenes(&list);
    for (size_t i = 0; i < list.sources.num; ++i) result << QString::fromUtf8(obs_source_get_name(list.sources.array[i]));
    obs_frontend_source_list_free(&list);
  } else {
    obs_source_t *sceneSource = obs_frontend_get_current_scene();
    if (!sceneSource) return result;
    obs_scene_t *scene = obs_scene_from_source(sceneSource);
    if (scene) {
      obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *data) {
        auto *out = static_cast<QStringList *>(data);
        obs_source_t *source = obs_sceneitem_get_source(item);
        if (source) out->append(QString::fromUtf8(obs_source_get_name(source)));
        return true;
      }, &result);
    }
    obs_source_release(sceneSource);
  }
  result.removeDuplicates();
  result.sort(Qt::CaseInsensitive);
  for (const auto &entry : entries) result.removeAll(entry.name);
  return result;
}

void SourceDeckDock::chooseAndAdd()
{
  const QStringList choices = availableItems();
  if (choices.isEmpty()) return;
  bool ok = false;
  const QString selected = QInputDialog::getItem(this, mode == Mode::Scenes ? "Add Scene" : "Add Source", mode == Mode::Scenes ? "Scene:" : "Source:", choices, 0, false, &ok);
  if (!ok || selected.isEmpty()) return;
  const int n = entries.size();
  entries.push_back({nullptr, selected, QPoint(18 + (n % 3) * 132, 18 + (n / 3) * 80)});
  saveLayout();
  rebuildButtons();
}

void SourceDeckDock::rebuildButtons()
{
  // Remove every existing deck button from the canvas immediately.  Using
  // deleteLater() alone leaves the old widgets visible until Qt returns to
  // the event loop, which produced overlapping "ghost" buttons after delete.
  const auto oldButtons = canvas->findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly);
  for (QPushButton *button : oldButtons) {
    button->removeEventFilter(this);
    button->hide();
    delete button;
  }
  draggingButton = nullptr;
  for (auto &entry : entries) entry.button = nullptr;
  for (int i = 0; i < entries.size(); ++i) addDeckButton(entries[i].name, i);
  syncStates();
}

void SourceDeckDock::addDeckButton(const QString &name, int index)
{
  auto *button = new QPushButton(name, canvas);
  button->setFixedSize(120, 68);
  button->setCheckable(mode == Mode::Sources);
  button->setProperty("deckIndex", index);
  button->installEventFilter(this);
  button->move(clampPosition(entries[index].pos, button->size()));
  button->show();
  connect(button, &QPushButton::clicked, this, [this, button]() {
    if (!editMode) activateEntry(button->property("deckIndex").toInt());
  });
  entries[index].button = button;
  updateButtonAppearance();
}

bool SourceDeckDock::eventFilter(QObject *watched, QEvent *event)
{
  auto *button = qobject_cast<QPushButton *>(watched);
  if (!button || !editMode) return QWidget::eventFilter(watched, event);
  const int index = button->property("deckIndex").toInt();
  if (index < 0 || index >= entries.size()) return QWidget::eventFilter(watched, event);
  if (event->type() == QEvent::MouseButtonPress) {
    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() == Qt::LeftButton) {
      draggingButton = button;
      dragOffset = mouse->position().toPoint();
      button->raise();
      return true;
    }
    if (mouse->button() == Qt::RightButton) {
      removeEntry(index);
      return true;
    }
  }
  if (event->type() == QEvent::MouseMove && draggingButton == button) {
    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->buttons() & Qt::LeftButton) {
      const QPoint next = clampPosition(canvas->mapFromGlobal(mouse->globalPosition().toPoint()) - dragOffset, button->size());
      button->move(next);
      entries[index].pos = next;
      return true;
    }
  }
  if (event->type() == QEvent::MouseButtonRelease && draggingButton == button) {
    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() == Qt::LeftButton) {
      entries[index].pos = button->pos();
      draggingButton = nullptr;
      saveLayout();
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}

QPoint SourceDeckDock::clampPosition(const QPoint &pos, const QSize &buttonSize) const
{
  if (!canvas) return pos;
  return QPoint(qBound(0, pos.x(), qMax(0, canvas->width() - buttonSize.width())), qBound(0, pos.y(), qMax(0, canvas->height() - buttonSize.height())));
}

void SourceDeckDock::removeEntry(int index)
{
  if (index < 0 || index >= entries.size()) return;
  // Hide the clicked button before mutating the model so deletion is visually immediate.
  if (entries[index].button) entries[index].button->hide();
  if (draggingButton == entries[index].button) draggingButton = nullptr;
  entries.removeAt(index);
  saveLayout();
  rebuildButtons();
}

void SourceDeckDock::activateEntry(int index)
{
  if (index < 0 || index >= entries.size()) return;
  const QString name = entries[index].name;
  if (mode == Mode::Scenes) {
    obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
    if (scene) { obs_frontend_set_current_scene(scene); obs_source_release(scene); }
    return;
  }
  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  if (!sceneSource) return;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (scene) {
    obs_sceneitem_t *item = obs_scene_find_source(scene, name.toUtf8().constData());
    if (item) obs_sceneitem_set_visible(item, !obs_sceneitem_visible(item));
  }
  obs_source_release(sceneSource);
}

void SourceDeckDock::updateButtonAppearance()
{
  for (auto &entry : entries) {
    if (!entry.button) continue;
    if (mode == Mode::Scenes) {
      const bool active = entry.name == currentSceneName();
      entry.button->setStyleSheet(editMode ? "QPushButton{font-weight:700;padding:6px;border:2px dashed #d38b2c;background:rgba(211,139,44,0.12);}" : active ? "QPushButton{font-weight:700;padding:6px;background:#5b7cfa;color:white;border:2px solid #5b7cfa;}" : "QPushButton{font-weight:600;padding:6px;border:2px solid #5b7cfa;}");
    } else {
      entry.button->setStyleSheet(editMode ? "QPushButton{font-weight:700;padding:6px;border:2px dashed #d38b2c;background:rgba(211,139,44,0.12);}" : "QPushButton{font-weight:600;padding:6px;} QPushButton:checked{background:#3a9d5d;color:white;}");
    }
  }
}

void SourceDeckDock::rebuild() { rebuildButtons(); }

void SourceDeckDock::syncStates()
{
  if (mode == Mode::Scenes) { updateButtonAppearance(); return; }
  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  obs_scene_t *scene = sceneSource ? obs_scene_from_source(sceneSource) : nullptr;
  for (auto &entry : entries) {
    if (!entry.button) continue;
    obs_sceneitem_t *item = scene ? obs_scene_find_source(scene, entry.name.toUtf8().constData()) : nullptr;
    entry.button->setEnabled(item != nullptr || editMode);
    QSignalBlocker blocker(entry.button);
    entry.button->setChecked(item && obs_sceneitem_visible(item));
  }
  if (sceneSource) obs_source_release(sceneSource);
  updateButtonAppearance();
}

void SourceDeckDock::saveLayout()
{
  QJsonArray array;
  for (const auto &entry : entries) {
    QJsonObject obj; obj["name"] = entry.name; obj["x"] = entry.pos.x(); obj["y"] = entry.pos.y(); array.append(obj);
  }
  QFile file(settingsPath());
  if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
}

void SourceDeckDock::loadLayout()
{
  QFile file(settingsPath());
  if (!file.open(QIODevice::ReadOnly)) return;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isArray()) return;
  int fallback = 0;
  for (const auto &value : doc.array()) {
    const QJsonObject obj = value.toObject();
    const QString name = obj["name"].toString();
    if (name.isEmpty()) continue;
    const QPoint pos = obj.contains("x") && obj.contains("y") ? QPoint(obj["x"].toInt(), obj["y"].toInt()) : QPoint(18 + (fallback % 3) * 132, 18 + (fallback / 3) * 80);
    entries.push_back({nullptr, name, pos});
    ++fallback;
  }
}
