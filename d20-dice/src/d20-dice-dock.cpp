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
#include <QJsonObject>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

static constexpr const char *DiceSourceName = "DnD Dice Roller";

static QString jsonIntArray(const QVector<int> &values)
{
  QStringList parts;
  for (int value : values)
    parts << QString::number(value);
  return "[" + parts.join(',') + "]";
}

static QString jsonBoolArray(const QVector<bool> &values)
{
  QStringList parts;
  for (bool value : values)
    parts << (value ? "true" : "false");
  return "[" + parts.join(',') + "]";
}

static QString htmlForRoll(const QVector<int> &sides, const QVector<int> &results,
                           const QVector<bool> &rolling, const QVector<bool> &locked,
                           qint64 nonce)
{
  int total = 0;
  for (int value : results)
    if (value > 0)
      total += value;

  return QString(R"HTML(<!doctype html><html><head><meta charset="utf-8"><style>
html,body{margin:0;width:100%;height:100%;overflow:hidden;background:transparent;font-family:Arial,sans-serif}
#stage{box-sizing:border-box;width:100%;height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:26px;padding:34px}
#dice{width:100%;display:flex;flex-wrap:wrap;justify-content:center;align-items:flex-end;gap:24px}
.dieWrap{position:relative;width:160px;height:190px;display:flex;flex-direction:column;align-items:center;justify-content:flex-end}
.die{position:relative;width:142px;height:142px;display:flex;align-items:center;justify-content:center;background:linear-gradient(145deg,#63749a,#27324f);border:4px solid rgba(255,255,255,.94);box-sizing:border-box;filter:drop-shadow(0 12px 12px rgba(0,0,0,.42));}
.dieValue{position:relative;z-index:3;color:#fff;font-size:48px;font-weight:900;line-height:1;text-shadow:0 3px 8px rgba(0,0,0,.9)}
.dieLabel{margin-top:8px;color:white;font-size:21px;font-weight:800;text-shadow:0 2px 5px #000}
.lockBadge{position:absolute;right:0;top:0;z-index:5;padding:5px 8px;border-radius:9px;background:rgba(10,12,18,.86);border:1px solid rgba(255,255,255,.7);color:white;font-size:13px;font-weight:800;display:none}
.locked .lockBadge{display:block}.locked .die{filter:drop-shadow(0 12px 12px rgba(0,0,0,.42)) grayscale(.4);opacity:.82}
.s3,.s4{clip-path:polygon(50% 2%,98% 96%,2% 96%);padding-top:26px}
.s6{border-radius:22px}
.s8{clip-path:polygon(50% 1%,98% 50%,50% 99%,2% 50%)}
.s10{clip-path:polygon(50% 1%,88% 20%,99% 55%,72% 98%,28% 98%,1% 55%,12% 20%)}
.s12{clip-path:polygon(26% 2%,74% 2%,98% 28%,93% 76%,50% 99%,7% 76%,2% 28%)}
.s20{clip-path:polygon(50% 1%,78% 10%,98% 34%,94% 70%,70% 96%,30% 96%,6% 70%,2% 34%,22% 10%)}
.s100{clip-path:polygon(31% 2%,69% 2%,92% 18%,100% 50%,92% 82%,69% 98%,31% 98%,8% 82%,0 50%,8% 18%)}
.rolling .dieValue{animation:pulse .22s ease-in-out infinite alternate}
@keyframes pulse{from{transform:scale(.93);opacity:.78}to{transform:scale(1.08);opacity:1}}
.total{min-width:280px;padding:14px 30px;border-radius:16px;background:rgba(15,18,28,.86);border:3px solid rgba(255,255,255,.92);color:white;text-align:center;font-size:26px;font-weight:800;text-shadow:0 2px 5px #000;opacity:0;transform:scale(.9);transition:.22s ease}
.total.show{opacity:1;transform:scale(1)}.total strong{font-size:60px;display:block;line-height:1.05}
</style></head><body><div id="stage"><div id="dice"></div><div id="total" class="total">TOTAL<strong>%6</strong></div></div><script>
const sides=%1, finals=%2, rolling=%3, locked=%4, nonce=%5;
const root=document.getElementById('dice');
const items=[];
for(let i=0;i<sides.length;i++){
  const wrap=document.createElement('div');
  wrap.className='dieWrap'+(locked[i]?' locked':'')+(rolling[i]?' rolling':'');
  const die=document.createElement('div');die.className='die s'+sides[i];
  const value=document.createElement('div');value.className='dieValue';
  value.textContent=rolling[i]?'?':(finals[i]>0?finals[i]:'-');
  const badge=document.createElement('div');badge.className='lockBadge';badge.textContent='LOCK';
  const label=document.createElement('div');label.className='dieLabel';label.textContent='D'+sides[i];
  die.appendChild(value);wrap.appendChild(die);wrap.appendChild(badge);wrap.appendChild(label);root.appendChild(wrap);
  items.push({wrap,value,sides:sides[i],final:finals[i],rolling:rolling[i]});
}
const hasRolling=rolling.some(Boolean);
if(hasRolling){
  const started=performance.now();
  const timer=setInterval(()=>{
    const elapsed=performance.now()-started;
    for(const item of items){
      if(item.rolling && elapsed < 3000)
        item.value.textContent=1+Math.floor(Math.random()*item.sides);
    }
    if(elapsed>=3000){
      clearInterval(timer);
      for(const item of items){
        if(item.rolling){item.value.textContent=item.final>0?item.final:'-';item.wrap.classList.remove('rolling');}
      }
      document.getElementById('total').classList.add('show');
    }
  },70);
}else{
  document.getElementById('total').classList.add('show');
}
</script></body></html>)HTML")
    .arg(jsonIntArray(sides))
    .arg(jsonIntArray(results))
    .arg(jsonBoolArray(rolling))
    .arg(jsonBoolArray(locked))
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
  scroll->setMinimumHeight(130);
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
  rollButton = new QPushButton("Roll Unlocked", this);
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
  if (diceSides.isEmpty()) {
    diceSides.push_back(20);
    diceLocked.push_back(false);
  }
  normalizeResultState();
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

void D20DiceDock::normalizeResultState()
{
  while (diceLocked.size() < diceSides.size()) diceLocked.push_back(false);
  while (diceLocked.size() > diceSides.size()) diceLocked.removeLast();
  while (lastResults.size() < diceSides.size()) lastResults.push_back(0);
  while (lastResults.size() > diceSides.size()) lastResults.removeLast();
}

void D20DiceDock::saveDiceConfig()
{
  const QString path = configPath();
  if (path.isEmpty()) return;
  QDir().mkpath(QFileInfo(path).absolutePath());
  QJsonArray array;
  for (int i = 0; i < diceSides.size(); ++i) {
    QJsonObject obj;
    obj["sides"] = diceSides[i];
    obj["locked"] = i < diceLocked.size() ? diceLocked[i] : false;
    array.append(obj);
  }
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
    int sides = 0;
    bool locked = false;
    if (value.isObject()) {
      const QJsonObject obj = value.toObject();
      sides = obj["sides"].toInt();
      locked = obj["locked"].toBool(false);
    } else {
      sides = value.toInt();
    }
    if (sides == 3 || sides == 4 || sides == 6 || sides == 8 || sides == 10 || sides == 12 || sides == 20 || sides == 100) {
      diceSides.push_back(sides);
      diceLocked.push_back(locked);
    }
  }
}

void D20DiceDock::addDie(int sides)
{
  diceSides.push_back(sides);
  diceLocked.push_back(false);
  lastResults.push_back(0);
  saveDiceConfig();
  rebuildDiceList();
}

void D20DiceDock::removeDie(int index)
{
  if (index < 0 || index >= diceSides.size()) return;
  diceSides.removeAt(index);
  if (index < diceLocked.size()) diceLocked.removeAt(index);
  if (index < lastResults.size()) lastResults.removeAt(index);
  saveDiceConfig();
  rebuildDiceList();
}

void D20DiceDock::toggleDieLock(int index)
{
  if (index < 0 || index >= diceSides.size()) return;
  normalizeResultState();
  diceLocked[index] = !diceLocked[index];
  saveDiceConfig();
  rebuildDiceList();
}

void D20DiceDock::rebuildDiceList()
{
  normalizeResultState();
  while (QLayoutItem *item = diceListLayout->takeAt(0)) {
    if (item->widget()) delete item->widget();
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
      const QString resultText = lastResults[i] > 0 ? QString(" = %1").arg(lastResults[i]) : QString();
      auto *label = new QLabel(QString("Die %1  •  D%2%3").arg(i + 1).arg(diceSides[i]).arg(resultText), rowWidget);
      auto *lock = new QPushButton(diceLocked[i] ? "Locked" : "Unlocked", rowWidget);
      auto *remove = new QPushButton("Remove", rowWidget);
      lock->setCheckable(true);
      lock->setChecked(diceLocked[i]);
      lock->setMaximumWidth(92);
      remove->setMaximumWidth(82);
      if (diceLocked[i])
        lock->setStyleSheet("QPushButton{font-weight:700;background:#72552b;} QPushButton:checked{background:#72552b;}");
      row->addWidget(label, 1);
      row->addWidget(lock);
      row->addWidget(remove);
      connect(lock, &QPushButton::clicked, this, [this, i]() { toggleDieLock(i); });
      connect(remove, &QPushButton::clicked, this, [this, i]() { removeDie(i); });
      diceListLayout->addWidget(rowWidget);
    }
  }
  diceListLayout->addStretch(1);
}

bool D20DiceDock::ensureDiceSource(const QVector<int> &results, const QVector<bool> &rolling, qint64 nonce)
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
  file.write(htmlForRoll(diceSides, results, rolling, diceLocked, nonce).toUtf8());
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
    obs_source_media_restart(source);
  }

  obs_sceneitem_t *item = obs_scene_find_source(scene, DiceSourceName);
  if (!item) item = obs_scene_add(scene, source);
  if (item) obs_sceneitem_set_visible(item, true);

  const QString oldPath = currentHtmlPath;
  currentHtmlPath = path;
  if (!oldPath.isEmpty() && oldPath != path)
    QTimer::singleShot(5000, this, [oldPath]() { QFile::remove(oldPath); });

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

  normalizeResultState();
  QVector<bool> rolling;
  rolling.reserve(diceSides.size());
  bool anyRolling = false;
  int total = 0;
  QStringList detail;

  for (int i = 0; i < diceSides.size(); ++i) {
    const bool shouldRoll = !diceLocked[i];
    rolling.push_back(shouldRoll);
    if (shouldRoll) {
      lastResults[i] = QRandomGenerator::global()->bounded(1, diceSides[i] + 1);
      anyRolling = true;
    }
    if (lastResults[i] > 0)
      total += lastResults[i];
    detail << QString("D%1: %2%3")
                  .arg(diceSides[i])
                  .arg(lastResults[i] > 0 ? QString::number(lastResults[i]) : "-")
                  .arg(diceLocked[i] ? " [LOCK]" : "");
  }

  if (!anyRolling) {
    resultLabel->setText("All dice are locked. Unlock at least one die to reroll.");
    totalLabel->setText(QString("TOTAL: %1").arg(total));
    return;
  }

  const qint64 nonce = QDateTime::currentMSecsSinceEpoch();
  activeRollNonce = nonce;
  if (ensureDiceSource(lastResults, rolling, nonce)) {
    resultLabel->setText("Rolling... results reveal in 3 seconds");
    totalLabel->setText("TOTAL: ...");
    rollButton->setText("Reroll Unlocked");
    rollButton->setEnabled(false);

    const QString finalDetail = detail.join("   |   ");
    QTimer::singleShot(3000, this, [this, nonce, finalDetail, total]() {
      if (activeRollNonce != nonce) return;
      resultLabel->setText(finalDetail);
      totalLabel->setText(QString("TOTAL: %1").arg(total));
      rollButton->setEnabled(true);
      rebuildDiceList();
    });
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
