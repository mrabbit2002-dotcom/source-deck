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
#stage{box-sizing:border-box;width:100%;height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:22px;padding:28px}
#dice{width:100%;display:flex;flex-wrap:wrap;justify-content:center;align-items:flex-end;gap:16px}
.dieWrap{position:relative;width:158px;height:188px;display:flex;flex-direction:column;align-items:center;justify-content:flex-end}
.dieCanvas{width:150px;height:150px;display:block;filter:drop-shadow(0 12px 10px rgba(0,0,0,.38))}
.dieLabel{margin-top:-5px;color:white;font-size:20px;font-weight:800;text-shadow:0 2px 5px #000}
.dieValue{position:absolute;left:50%;top:68px;transform:translate(-50%,-50%);color:white;font-size:40px;font-weight:900;text-shadow:0 3px 8px #000;pointer-events:none}
.lockBadge{position:absolute;right:8px;top:8px;padding:5px 8px;border-radius:9px;background:rgba(10,12,18,.82);border:1px solid rgba(255,255,255,.65);color:white;font-size:13px;font-weight:800;display:none}
.locked .lockBadge{display:block}
.locked .dieCanvas{filter:drop-shadow(0 12px 10px rgba(0,0,0,.38)) grayscale(.32)}
.total{min-width:260px;padding:14px 30px;border-radius:16px;background:rgba(15,18,28,.84);border:3px solid rgba(255,255,255,.9);color:white;text-align:center;font-size:26px;font-weight:800;text-shadow:0 2px 5px #000;opacity:0;transform:scale(.88);transition:.25s ease}
.total.show{opacity:1;transform:scale(1)}
.total strong{font-size:58px;display:block;line-height:1.05}
</style></head><body><div id="stage"><div id="dice"></div><div id="total" class="total">TOTAL<strong>%6</strong></div></div><script>
const sides=%1, finals=%2, rolling=%3, locked=%4, nonce=%5;
const root=document.getElementById('dice');
const items=[];

function meshFor(s){
  if(s===4){return {v:[[1,1,1],[-1,-1,1],[-1,1,-1],[1,-1,-1]],f:[[0,1,2],[0,3,1],[0,2,3],[1,3,2]]};}
  if(s===6){return {v:[[-1,-1,-1],[1,-1,-1],[1,1,-1],[-1,1,-1],[-1,-1,1],[1,-1,1],[1,1,1],[-1,1,1]],f:[[0,1,2,3],[4,7,6,5],[0,4,5,1],[1,5,6,2],[2,6,7,3],[4,0,3,7]]};}
  if(s===8){return {v:[[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]],f:[[0,2,4],[2,1,4],[1,3,4],[3,0,4],[2,0,5],[1,2,5],[3,1,5],[0,3,5]]};}
  if(s===3){return {v:[[-1,-.85,-1],[1,-.85,-1],[0,1,-1],[-1,-.85,1],[1,-.85,1],[0,1,1]],f:[[0,2,1],[3,4,5],[0,1,4,3],[1,2,5,4],[2,0,3,5]]};}
  if(s===10||s===100){const v=[[0,1.55,0],[0,-1.55,0]];for(let i=0;i<5;i++){const a=i*Math.PI*2/5;v.push([Math.cos(a)*1.18,0,Math.sin(a)*1.18]);}const f=[];for(let i=0;i<5;i++){const a=2+i,b=2+((i+1)%5);f.push([0,a,b]);f.push([1,b,a]);}return {v,f};}
  if(s===12){const p=(1+Math.sqrt(5))/2, q=1/p;const v=[];for(const a of [-1,1])for(const b of [-1,1])for(const c of [-1,1])v.push([a,b,c]);for(const a of [-1,1])for(const b of [-1,1]){v.push([0,a*q,b*p]);v.push([a*q,b*p,0]);v.push([b*p,0,a*q]);}const f=[];for(let i=0;i<v.length;i+=3)f.push([i,(i+1)%v.length,(i+2)%v.length]);return {v,f};}
  const p=(1+Math.sqrt(5))/2;const v=[[-1,p,0],[1,p,0],[-1,-p,0],[1,-p,0],[0,-1,p],[0,1,p],[0,-1,-p],[0,1,-p],[p,0,-1],[p,0,1],[-p,0,-1],[-p,0,1]];
  const f=[[0,11,5],[0,5,1],[0,1,7],[0,7,10],[0,10,11],[1,5,9],[5,11,4],[11,10,2],[10,7,6],[7,1,8],[3,9,4],[3,4,2],[3,2,6],[3,6,8],[3,8,9],[4,9,5],[2,4,11],[6,2,10],[8,6,7],[9,8,1]];return {v,f};
}
function rot(v,ax,ay,az){let[x,y,z]=v;let c=Math.cos(ax),q=Math.sin(ax);[y,z]=[y*c-z*q,y*q+z*c];c=Math.cos(ay);q=Math.sin(ay);[x,z]=[x*c+z*q,-x*q+z*c];c=Math.cos(az);q=Math.sin(az);[x,y]=[x*c-y*q,x*q+y*c];return[x,y,z];}
function drawDie(item,t){
  const c=item.canvas,ctx=c.getContext('2d'),m=item.mesh;ctx.clearRect(0,0,c.width,c.height);
  const active=item.rolling, dur=1900, e=Math.min(1,t/dur), ease=1-Math.pow(1-e,3);
  const spin=active?(t*.0105*(1-e*.62)):.0;
  const ax=(active?spin*.83:1.05)+item.seed*.17, ay=(active?spin*1.08:1.42)+item.seed*.11, az=(active?spin*.61:.38)+item.seed*.07;
  const bounce=active?(Math.abs(Math.sin(t*.011))*24*(1-e)):0;
  const scale=active?(1+.14*Math.sin(Math.min(1,e)*Math.PI)):1;
  ctx.save();ctx.translate(0,-bounce);
  ctx.beginPath();ctx.ellipse(75,132,42*(1-e*.25),11*(1-e*.25),0,0,Math.PI*2);ctx.fillStyle='rgba(0,0,0,.30)';ctx.fill();
  const p=m.v.map(v=>{const r=rot(v,ax,ay,az),d=4.8+r[2];return [75+r[0]*108/d*scale,70+r[1]*108/d*scale,r[2]];});
  const fs=m.f.map(f=>({f,z:f.reduce((a,i)=>a+p[i][2],0)/f.length})).sort((a,b)=>a.z-b.z);
  for(const o of fs){const f=o.f;ctx.beginPath();ctx.moveTo(p[f[0]][0],p[f[0]][1]);for(let j=1;j<f.length;j++)ctx.lineTo(p[f[j]][0],p[f[j]][1]);ctx.closePath();const shade=Math.max(28,Math.min(72,46+o.z*13));ctx.fillStyle=`hsl(222 36% ${shade}%)`;ctx.fill();ctx.lineWidth=2.1;ctx.strokeStyle='rgba(255,255,255,.84)';ctx.stroke();}
  ctx.restore();
  if(active&&t<dur){item.value.textContent=1+Math.floor(Math.random()*item.sides);}else{item.value.textContent=item.final>0?item.final:'-';}
}
for(let i=0;i<sides.length;i++){
  const wrap=document.createElement('div');wrap.className='dieWrap'+(locked[i]?' locked':'');
  const canvas=document.createElement('canvas');canvas.className='dieCanvas';canvas.width=150;canvas.height=150;
  const value=document.createElement('div');value.className='dieValue';value.textContent=finals[i]>0?finals[i]:'-';
  const badge=document.createElement('div');badge.className='lockBadge';badge.textContent='LOCK';
  const label=document.createElement('div');label.className='dieLabel';label.textContent='D'+sides[i];
  wrap.appendChild(canvas);wrap.appendChild(value);wrap.appendChild(badge);wrap.appendChild(label);root.appendChild(wrap);
  items.push({canvas,value,sides:sides[i],final:finals[i],rolling:rolling[i],mesh:meshFor(sides[i]),seed:(i+1)*.731});
}
let start=performance.now();
function frame(now){const t=now-start;for(const item of items)drawDie(item,t);if(t<2050&&rolling.some(Boolean))requestAnimationFrame(frame);else{for(const item of items){item.rolling=false;drawDie(item,99999);}document.getElementById('total').classList.add('show');}}
requestAnimationFrame(frame);
if(!rolling.some(Boolean))document.getElementById('total').classList.add('show');
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
  if (ensureDiceSource(lastResults, rolling, nonce)) {
    resultLabel->setText(detail.join("   |   "));
    totalLabel->setText(QString("TOTAL: %1").arg(total));
    rollButton->setText("Reroll Unlocked");
    rebuildDiceList();
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
