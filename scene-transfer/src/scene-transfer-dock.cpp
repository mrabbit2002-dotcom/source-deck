#include "scene-transfer-dock.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSet>
#include <QVBoxLayout>

namespace {
struct CollectContext {
  QSet<QString> names;
};

static void collectSourceTree(obs_source_t *source, CollectContext &ctx)
{
  if (!source) return;
  const char *rawName = obs_source_get_name(source);
  if (!rawName || !*rawName) return;
  const QString name = QString::fromUtf8(rawName);
  if (ctx.names.contains(name)) return;
  ctx.names.insert(name);

  obs_scene_t *scene = obs_scene_from_source(source);
  if (!scene) return;

  obs_scene_enum_items(scene, [](obs_scene_t *, obs_sceneitem_t *item, void *data) {
    auto *ctx = static_cast<CollectContext *>(data);
    collectSourceTree(obs_sceneitem_get_source(item), *ctx);
    return true;
  }, &ctx);
}

static bool saveFilter(void *data, obs_source_t *source)
{
  auto *ctx = static_cast<CollectContext *>(data);
  const char *name = obs_source_get_name(source);
  return name && ctx->names.contains(QString::fromUtf8(name));
}

static QStringList sourceNamesInArray(obs_data_array_t *array)
{
  QStringList names;
  if (!array) return names;
  const size_t count = obs_data_array_count(array);
  for (size_t i = 0; i < count; ++i) {
    obs_data_t *item = obs_data_array_item(array, i);
    const char *name = obs_data_get_string(item, "name");
    if (name && *name) names << QString::fromUtf8(name);
    obs_data_release(item);
  }
  return names;
}
}

SceneTransferDock::SceneTransferDock(QWidget *parent) : QWidget(parent)
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

  auto *sceneRow = new QHBoxLayout();
  sceneCombo = new QComboBox(this);
  refreshButton = new QPushButton("Refresh", this);
  sceneRow->addWidget(sceneCombo, 1);
  sceneRow->addWidget(refreshButton);
  root->addLayout(sceneRow);

  exportButton = new QPushButton("Export Selected Scene", this);
  importButton = new QPushButton("Import Scene Package", this);
  exportButton->setMinimumHeight(42);
  importButton->setMinimumHeight(42);
  root->addWidget(exportButton);
  root->addWidget(importButton);

  statusLabel = new QLabel("Export a scene to a portable JSON package, or import one into this scene collection.", this);
  statusLabel->setWordWrap(true);
  root->addWidget(statusLabel);
  root->addStretch(1);

  connect(refreshButton, &QPushButton::clicked, this, [this]() { refreshScenes(); });
  connect(exportButton, &QPushButton::clicked, this, [this]() { exportScene(); });
  connect(importButton, &QPushButton::clicked, this, [this]() { importScene(); });

  refreshScenes();
}

void SceneTransferDock::setStatus(const QString &text, bool error)
{
  statusLabel->setText(text);
  statusLabel->setStyleSheet(error ? "color:#e05a5a;font-weight:600;" : "");
}

void SceneTransferDock::refreshScenes()
{
  const QString previous = sceneCombo->currentText();
  sceneCombo->clear();
  obs_frontend_source_list list{};
  obs_frontend_get_scenes(&list);
  for (size_t i = 0; i < list.sources.num; ++i) {
    const char *name = obs_source_get_name(list.sources.array[i]);
    if (name) sceneCombo->addItem(QString::fromUtf8(name));
  }
  obs_frontend_source_list_free(&list);
  const int oldIndex = sceneCombo->findText(previous);
  if (oldIndex >= 0) sceneCombo->setCurrentIndex(oldIndex);
}

void SceneTransferDock::exportScene()
{
  const QString sceneName = sceneCombo->currentText();
  if (sceneName.isEmpty()) {
    setStatus("No scene selected.", true);
    return;
  }

  obs_source_t *sceneSource = obs_get_source_by_name(sceneName.toUtf8().constData());
  if (!sceneSource || !obs_scene_from_source(sceneSource)) {
    if (sceneSource) obs_source_release(sceneSource);
    setStatus("The selected scene could not be found.", true);
    return;
  }

  CollectContext ctx;
  collectSourceTree(sceneSource, ctx);
  obs_data_array_t *sources = obs_save_sources_filtered(saveFilter, &ctx);
  obs_source_release(sceneSource);
  if (!sources) {
    setStatus("Could not serialize scene sources.", true);
    return;
  }

  obs_data_t *package = obs_data_create();
  obs_data_set_string(package, "format", "obs-scene-transfer-v1");
  obs_data_set_string(package, "root_scene", sceneName.toUtf8().constData());
  obs_data_set_array(package, "sources", sources);

  QString suggested = sceneName;
  suggested.replace('/', '_');
  suggested.replace('\\', '_');
  const QString path = QFileDialog::getSaveFileName(this, "Export Scene", suggested + ".obs-scene.json", "OBS Scene Package (*.obs-scene.json *.json)");
  if (path.isEmpty()) {
    obs_data_release(package);
    obs_data_array_release(sources);
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    obs_data_release(package);
    obs_data_array_release(sources);
    setStatus("Could not write the selected file.", true);
    return;
  }
  const char *json = obs_data_get_json(package);
  file.write(json ? json : "{}");
  file.close();

  const int sourceCount = (int)obs_data_array_count(sources);
  obs_data_release(package);
  obs_data_array_release(sources);
  setStatus(QString("Exported '%1' with %2 source definition(s).").arg(sceneName).arg(sourceCount));
}

void SceneTransferDock::importScene()
{
  const QString path = QFileDialog::getOpenFileName(this, "Import Scene Package", QString(), "OBS Scene Package (*.obs-scene.json *.json)");
  if (path.isEmpty()) return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    setStatus("Could not open the selected file.", true);
    return;
  }
  const QByteArray bytes = file.readAll();
  file.close();

  obs_data_t *package = obs_data_create_from_json(bytes.constData());
  if (!package) {
    setStatus("This is not valid JSON.", true);
    return;
  }

  const QString format = QString::fromUtf8(obs_data_get_string(package, "format"));
  const QString rootScene = QString::fromUtf8(obs_data_get_string(package, "root_scene"));
  obs_data_array_t *sources = obs_data_get_array(package, "sources");
  if (format != "obs-scene-transfer-v1" || rootScene.isEmpty() || !sources) {
    if (sources) obs_data_array_release(sources);
    obs_data_release(package);
    setStatus("This file is not a Scene Transfer package.", true);
    return;
  }

  QStringList conflicts;
  const QStringList names = sourceNamesInArray(sources);
  for (const QString &name : names) {
    obs_source_t *existing = obs_get_source_by_name(name.toUtf8().constData());
    if (existing) {
      conflicts << name;
      obs_source_release(existing);
    }
  }

  if (!conflicts.isEmpty()) {
    obs_data_array_release(sources);
    obs_data_release(package);
    QMessageBox::warning(this, "Scene Transfer",
                         "Import stopped to protect your current collection.\n\n"
                         "These source names already exist:\n• " + conflicts.join("\n• ") +
                         "\n\nRename the conflicting source(s) in either collection and export again.");
    setStatus(QString("Import stopped: %1 conflicting source name(s).").arg(conflicts.size()), true);
    return;
  }

  obs_frontend_defer_save_begin();
  obs_load_sources(sources, nullptr, nullptr);
  obs_frontend_defer_save_end();
  obs_frontend_save();

  obs_source_t *importedScene = obs_get_source_by_name(rootScene.toUtf8().constData());
  if (importedScene) {
    obs_frontend_set_current_scene(importedScene);
    obs_source_release(importedScene);
  }

  const int sourceCount = (int)obs_data_array_count(sources);
  obs_data_array_release(sources);
  obs_data_release(package);
  refreshScenes();
  setStatus(QString("Imported '%1' with %2 source definition(s). Missing media paths may still need relinking.").arg(rootScene).arg(sourceCount));
}
