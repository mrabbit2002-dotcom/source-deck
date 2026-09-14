#include "d20-dice-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

static constexpr const char *DiceSourceName = "DnD Dice Roller";

static QString jsonArray(const QVector<int> &values)
{
  QStringList parts;
  for (int value : values)
    parts << QString::number(value);
  return "[" + parts.join(',') + "]";
}

static QString htmlForRoll(const QVector<int> &sides, const QVector<int> &results, qint64 nonce)
{
  int total = 0;
  for (int value : results)
    total += value;

  return QString(R"HTML(<!doctype html><html><head><meta charset="utf-8"><style>
html,body{margin:0;width:100%;height:100%;overflow:hidden;background:transparent;font-family:Arial,sans-serif}
#stage{box-sizing:border-box;width:100%;height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:24px;padding:34px}
#dice{width:100%;display:flex;flex-wrap:wrap;justify-content:center;align-items:center;gap:22px}
.dieWrap{width:150px;height:174px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;perspective:600px}
.die{width:126px;height:126px;display:flex;align-items:center;justify-content:center;color:white;font-size:44px;font-weight:900;text-shadow:0 3px 9px rgba(0,0,0,.9);border:4px solid rgba(255,255,255,.9);box-sizing:border-box;background:linear-gradient(145deg,rgba(73,91,135,.97),rgba(26,31,48,.97));filter:drop-shadow(0 10px 12px rgba(0,0,0,.45));transform-style:preserve-3d}
.die.rolling{animation:tumble 1.9s cubic-bezier(.15,.75,.15,1)}
.label{color:white;font-size:21px;font-weight:800;text-shadow:0 2px 5px #000}
.total{min-width:260px;padding:16px 30px;border-radius:16px;background:rgba(15,18,28,.82);border:3px solid rgba(255,255,255,.9);color:white;text-align:center;font-size:28px;font-weight:800;text-shadow:0 2px 5px #000;opacity:0;transform:scale(.85);transition:.28s ease}
.total.show{opacity:1;transform:scale(1)}
.total strong{font-size:58px;display:block;line-height:1.05}
.s3,.s4{clip-path:polygon(50% 0,100% 100%,0 100%)}
.s6{border-radius:16px}
.s8{clip-path:polygon(50% 0,100% 50%,50% 100%,0 50%)}
.s10{clip-path:polygon(50% 0,90% 20%,100% 62%,72% 100%,28% 100%,0 62%,10% 20%)}
.s12{clip-path:polygon(25% 4%,75% 4%,100% 30%,92% 78%,50% 100%,8% 78%,0 30%)}
.s20,.s100{clip-path:polygon(50% 0,80% 10%,100% 35%,95% 70%,70% 96%,30% 96%,5% 70%,0 35%,20% 10%)}
@keyframes tumble{0%{transform:rotateX(0) rotateY(0) rotateZ(0) scale(.72)}20%{transform:rotateX(210deg) rotateY(150deg) rotateZ(110deg) scale(1.08)}50%{transform:rotateX(520deg) rotateY(390deg) rotateZ(330deg) scale(.88)}78%{transform:rotateX(760deg) rotateY(650deg) rotateZ(540deg) scale(1.03)}100%{transform:rotateX(1080deg) rotateY(900deg) rotateZ(720deg) scale(1)}}
</style></head><body><div id="stage"><div id="dice"></div><div id="total" class="total">TOTAL<strong>%4</strong></div></div><script>
const sides=%1, finals=%2, nonce=%3;
const root=document.getElementById('dice');
const nodes=[];
for(let i=0;i<sides.length;i++){
  const wrap=document.createElement('div');wrap.className='dieWrap';
  const die=document.createElement('div');die.className='die rolling s'+sides[i];die.textContent='1';
  const label=document.createElement('div');label.className='label';label.textContent='D'+sides[i];
  wrap.appendChild(die);wrap.appendChild(label);root.appendChild(wrap);nodes.push(die);
}
let started=performance.now();
const timer=setInterval(()=>{
  const elapsed=performance.now()-started;
  nodes.forEach((n,i)=>{n.textContent=elapsed>1650?finals[i]:(1+Math.floor(Math.random()*sides[i]));});
  if(elapsed>1900){clearInterval(timer);nodes.forEach((n,i)=>{n.textContent=finals[i];n.classList.remove('rolling');});document.getElementById('total').classList.add('show');}
},55);
</script></body></html>)HTML")
    .arg(jsonArray(sides))
    .arg(jsonArray(results))
    .arg(nonce)
    .arg(total);
}

D20DiceDock::D20DiceDock(QWidget *parent) : QWidget(parent)
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

  auto *addRow = new QHBoxLayout();
  dieTypeCombo = new QComboBox(this);
  const QList<int> types{3, 4, 6, 8, 10, 12, 20, 100};
  for (int sides : types)
    dieTypeCombo->addItem(QString("D%1").arg(sides), sides);
  dieTypeCombo->setCurrentText("D20");
  addDieButton = new QPushButton("+ Add Die", this);
  addDieButton->setMinimumHeight(36);
  addRow->addWidget(dieTypeCombo, 1);
  addRow->addWidget(addDieButton);
  root->addLayout(addRow);

  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setMinimumHeight(110);
  diceListWidget = new QWidget(scroll);
  diceListLayout = new QVBoxLayout(diceListWidget);
  diceListLayout->setContentsMargins(4, 4, 4, 4);
  diceListLayout->setSpacing(4);
  scroll->setWidget(diceListWidget);
  root->addWidget(scroll, 1);

  resultLabel = new QLabel("Add dice, then roll.", this);
  resultLabel->setWordWrap(true);
  resultLabel->setAlignment(Qt::AlignCenter);
  resultLabel->setStyleSheet("font-weight:600;padding:6px;");
  root->addWidget(resultLabel);

  totalLabel = new QLabel("TOTAL: -", this);
  totalLabel->setAlignment(Qt::AlignCenter);
  totalLabel->setStyleSheet("font-size:22px;font-weight:800;padding:8px;");
  root->addWidget(totalLabel);

  auto *buttonRow = new QHBoxLayout();
  rollButton = new QPushButton("Roll All", this);
  visibilityButton = new QPushButton("Hide", this);
  rollButton->setMinimumHeight(42);
  visibilityButton->setMinimumHeight(42);
  buttonRow->addWidget(rollButton, 2);
  buttonRow->addWidget(visibilityButton, 1);
  root->addLayout(buttonRow);

  connect(addDieButton, &QPushButton::clicked, this, [this]() {
    addDie(dieTypeCombo->currentData().toInt());
  });
  connect(rollButton, &QPushButton::clicked, this, [this]() { roll(); });
  connect(visibilityButton, &QPushButton::clicked, this, [this]() { toggleVisibility(); });

  loadDiceConfig();
  if (diceSides.isEmpty())
    diceSides.push_back(20);
  rebuildDiceList();
  QTimer::singleShot(0, this, [this]() { updateVisibilityButton(); });
}

QString D20DiceDock::configPath() const
{
  char *raw = obs_module_config_path("dice-config.json");
  if (!raw)
    return {};
  const QString path = QString::fromUtf8(raw);
  bfree(raw);
  return path;
}

void D20DiceDock::saveDiceConfig()
{
  const QString path = configPath();
  if (path.isEmpty()) return;
  QDir().mkpath(QFileInfo(path).absolutePath());
  QJsonArray array;
  for (int sides : diceSides)
    array.append(sides);
  QFile file(path);
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void D20DiceDock::loadDiceConfig()
{
  const QString path = configPath();
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isArray()) return;
  for (const QJsonValue &value : doc.array()) {
    const int sides = value.toInt();
    if (sides == 3 || sides == 4 || sides == 6 || sides == 8 || sides == 10 || sides == 12 || sides == 20 || sides == 100)
      diceSides.push_back(sides);
  }
}

void D20DiceDock::addDie(int sides)
{
  diceSides.push_back(sides);
  saveDiceConfig();
  rebuildDiceList();
}

void D20DiceDock::removeDie(int index)
{
  if (index < 0 || index >= diceSides.size()) return;
  diceSides.removeAt(index);
  saveDiceConfig();
  rebuildDiceList();
}

void D20DiceDock::rebuildDiceList()
{
  while (QLayoutItem *item = diceListLayout->takeAt(0)) {
    if (item->widget())
      delete item->widget();
    delete item;
  }

  if (diceSides.isEmpty()) {
    auto *empty = new QLabel("No dice. Choose a die type and click + Add Die.", diceListWidget);
    empty->setAlignment(Qt::AlignCenter);
    diceListLayout->addWidget(empty);
  } else {
    for (int i = 0; i < diceSides.size(); ++i) {
      auto *rowWidget = new QWidget(diceListWidget);
      auto *row = new QHBoxLayout(rowWidget);
      row->setContentsMargins(4, 2, 4, 2);
      auto *label = new QLabel(QString("Die %1   •   D%2").arg(i + 1).arg(diceSides[i]), rowWidget);
      auto *remove = new QPushButton("Remove", rowWidget);
      remove->setMaximumWidth(88);
      row->addWidget(label, 1);
      row->addWidget(remove);
      connect(remove, &QPushButton::clicked, this, [this, i]() { removeDie(i); });
      diceListLayout->addWidget(rowWidget);
    }
  }
  diceListLayout->addStretch(1);
}

bool D20DiceDock::ensureDiceSource(const QVector<int> &results, qint64 nonce)
{
  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  if (!sceneSource) return false;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (!scene) { obs_source_release(sceneSource); return false; }

  const QString fileName = QString("dice-roll-%1.html").arg(nonce);
  const QByteArray fileNameUtf8 = fileName.toUtf8();
  char *pathRaw = obs_module_config_path(fileNameUtf8.constData());
  if (!pathRaw) { obs_source_release(sceneSource); return false; }
  const QString path = QString::fromUtf8(pathRaw);
  bfree(pathRaw);
  QDir().mkpath(QFileInfo(path).absolutePath());

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    obs_source_release(sceneSource);
    return false;
  }
  file.write(htmlForRoll(diceSides, results, nonce).toUtf8());
  file.close();

  obs_source_t *source = obs_get_source_by_name(DiceSourceName);
  if (!source) {
    obs_data_t *settings = obs_data_create();
    obs_data_set_bool(settings, "is_local_file", true);
    obs_data_set_string(settings, "local_file", path.toUtf8().constData());
    obs_data_set_int(settings, "width", 1100);
    obs_data_set_int(settings, "height", 700);
    obs_data_set_int(settings, "fps", 60);
    source = obs_source_create("browser_source", DiceSourceName, settings, nullptr);
    obs_data_release(settings);
    if (!source) { obs_source_release(sceneSource); return false; }
  } else {
    obs_data_t *settings = obs_source_get_settings(source);
    obs_data_set_bool(settings, "is_local_file", true);
    obs_data_set_string(settings, "local_file", path.toUtf8().constData());
    obs_data_set_int(settings, "width", 1100);
    obs_data_set_int(settings, "height", 700);
    obs_data_set_int(settings, "fps", 60);
    obs_source_update(source, settings);
    obs_data_release(settings);
    // A unique local-file path on every roll prevents CEF from reusing a cached
    // document. Restarting media as well makes rapid rerolls reliably replay.
    obs_source_media_restart(source);
  }

  obs_sceneitem_t *item = obs_scene_find_source(scene, DiceSourceName);
  if (!item)
    item = obs_scene_add(scene, source);
  if (item)
    obs_sceneitem_set_visible(item, true);

  const QString oldPath = currentHtmlPath;
  currentHtmlPath = path;
  if (!oldPath.isEmpty() && oldPath != path) {
    QTimer::singleShot(5000, this, [oldPath]() { QFile::remove(oldPath); });
  }

  obs_source_release(source);
  obs_source_release(sceneSource);
  return item != nullptr;
}

void D20DiceDock::roll()
{
  if (diceSides.isEmpty()) {
    resultLabel->setText("Add at least one die first.");
    totalLabel->setText("TOTAL: -");
    return;
  }

  QVector<int> results;
  results.reserve(diceSides.size());
  int total = 0;
  QStringList detail;
  for (int sides : diceSides) {
    const int result = QRandomGenerator::global()->bounded(1, sides + 1);
    results.push_back(result);
    total += result;
    detail << QString("D%1: %2").arg(sides).arg(result);
  }

  const qint64 nonce = QDateTime::currentMSecsSinceEpoch();
  if (ensureDiceSource(results, nonce)) {
    lastResults = results;
    resultLabel->setText(detail.join("   |   "));
    totalLabel->setText(QString("TOTAL: %1").arg(total));
    rollButton->setText("Reroll All");
  } else {
    resultLabel->setText("Could not create or refresh the Browser Source.");
  }
  updateVisibilityButton();
}

void D20DiceDock::toggleVisibility()
{
  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  if (!sceneSource) return;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (scene) {
    if (obs_sceneitem_t *item = obs_scene_find_source(scene, DiceSourceName))
      obs_sceneitem_set_visible(item, !obs_sceneitem_visible(item));
  }
  obs_source_release(sceneSource);
  updateVisibilityButton();
}

void D20DiceDock::updateVisibilityButton()
{
  bool visible = false;
  bool exists = false;
  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  if (sceneSource) {
    obs_scene_t *scene = obs_scene_from_source(sceneSource);
    if (scene) {
      if (obs_sceneitem_t *item = obs_scene_find_source(scene, DiceSourceName)) {
        exists = true;
        visible = obs_sceneitem_visible(item);
      }
    }
    obs_source_release(sceneSource);
  }
  visibilityButton->setEnabled(exists);
  visibilityButton->setText(visible ? "Hide" : "Show");
}
