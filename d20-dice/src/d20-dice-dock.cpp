#include "d20-dice-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>

static constexpr const char *DiceSourceName = "D20 Dice";

static QString htmlForRoll(int result, qint64 nonce)
{
  return QString(R"HTML(<!doctype html><html><head><meta charset="utf-8"><style>
html,body{margin:0;width:100%;height:100%;overflow:hidden;background:transparent}
canvas{width:100%;height:100%;display:block}
</style></head><body><canvas id="c" width="700" height="700"></canvas><script>
const finalResult=%1, seed=%2;
const c=document.getElementById('c'),ctx=c.getContext('2d');
let start=performance.now(),lastSwap=0,current=1;
const verts=[[-1,1.618,0],[1,1.618,0],[-1,-1.618,0],[1,-1.618,0],[0,-1,1.618],[0,1,1.618],[0,-1,-1.618],[0,1,-1.618],[1.618,0,-1], [1.618,0,1],[-1.618,0,-1],[-1.618,0,1]];
const faces=[[0,11,5],[0,5,1],[0,1,7],[0,7,10],[0,10,11],[1,5,9],[5,11,4],[11,10,2],[10,7,6],[7,1,8],[3,9,4],[3,4,2],[3,2,6],[3,6,8],[3,8,9],[4,9,5],[2,4,11],[6,2,10],[8,6,7],[9,8,1]];
function rot(v,ax,ay,az){let[x,y,z]=v;let c=Math.cos(ax),s=Math.sin(ax);[y,z]=[y*c-z*s,y*s+z*c];c=Math.cos(ay);s=Math.sin(ay);[x,z]=[x*c+z*s,-x*s+z*c];c=Math.cos(az);s=Math.sin(az);[x,y]=[x*c-y*s,x*s+y*c];return[x,y,z]}
function draw(t){ctx.clearRect(0,0,c.width,c.height);let e=Math.min(1,t/1900),spin=(1-e)*(7.5*t/1000)+e*3.2;let ax=spin*.73,ay=spin,az=spin*.41;let p=verts.map(v=>{let r=rot(v,ax,ay,az),d=4.8+r[2];return [350+r[0]*150/d*4,350+r[1]*150/d*4,r[2]]});
let fs=faces.map((f,i)=>({f,z:(p[f[0]][2]+p[f[1]][2]+p[f[2]][2])/3,i})).sort((a,b)=>a.z-b.z);ctx.lineWidth=4;
for(const o of fs){const f=o.f;let shade=Math.max(35,Math.min(85,55+o.z*12));ctx.beginPath();ctx.moveTo(p[f[0]][0],p[f[0]][1]);ctx.lineTo(p[f[1]][0],p[f[1]][1]);ctx.lineTo(p[f[2]][0],p[f[2]][1]);ctx.closePath();ctx.fillStyle=`hsla(220,25%,${shade}%,0.72)`;ctx.fill();ctx.strokeStyle='rgba(255,255,255,.82)';ctx.stroke()}
if(t-lastSwap>Math.max(55,210*e)){lastSwap=t;current=e>.92?finalResult:1+Math.floor(Math.random()*20)}
ctx.textAlign='center';ctx.textBaseline='middle';ctx.font='900 132px Arial';ctx.lineWidth=16;ctx.strokeStyle='rgba(0,0,0,.62)';ctx.strokeText(current,350,350);ctx.fillStyle='white';ctx.fillText(current,350,350);
if(t<2200)requestAnimationFrame(now=>draw(now-start));else{current=finalResult;drawFinal()}}
function drawFinal(){ctx.clearRect(0,0,c.width,c.height);let p=verts.map(v=>{let r=rot(v,2.34,3.1,.7),d=4.8+r[2];return [350+r[0]*150/d*4,350+r[1]*150/d*4,r[2]]});let fs=faces.map(f=>({f,z:(p[f[0]][2]+p[f[1]][2]+p[f[2]][2])/3})).sort((a,b)=>a.z-b.z);ctx.lineWidth=4;for(const o of fs){let f=o.f;ctx.beginPath();ctx.moveTo(p[f[0]][0],p[f[0]][1]);ctx.lineTo(p[f[1]][0],p[f[1]][1]);ctx.lineTo(p[f[2]][0],p[f[2]][1]);ctx.closePath();ctx.fillStyle='rgba(70,85,115,.86)';ctx.fill();ctx.strokeStyle='rgba(255,255,255,.9)';ctx.stroke()}ctx.textAlign='center';ctx.textBaseline='middle';ctx.font='900 132px Arial';ctx.lineWidth=16;ctx.strokeStyle='rgba(0,0,0,.62)';ctx.strokeText(finalResult,350,350);ctx.fillStyle='white';ctx.fillText(finalResult,350,350)}
requestAnimationFrame(now=>{start=now;draw(0)});
</script></body></html>)HTML").arg(result).arg(nonce);
}

D20DiceDock::D20DiceDock(QWidget *parent) : QWidget(parent)
{
  auto *root = new QVBoxLayout(this);
  resultLabel = new QLabel("Ready", this);
  resultLabel->setAlignment(Qt::AlignCenter);
  resultLabel->setStyleSheet("font-size:18px;font-weight:700;padding:8px;");
  root->addWidget(resultLabel);

  auto *row = new QHBoxLayout();
  rollButton = new QPushButton("Roll D20", this);
  visibilityButton = new QPushButton("Hide", this);
  rollButton->setMinimumHeight(42);
  visibilityButton->setMinimumHeight(42);
  row->addWidget(rollButton);
  row->addWidget(visibilityButton);
  root->addLayout(row);

  connect(rollButton, &QPushButton::clicked, this, [this]() { roll(); });
  connect(visibilityButton, &QPushButton::clicked, this, [this]() { toggleVisibility(); });
  QTimer::singleShot(0, this, [this]() { updateVisibilityButton(); });
}

bool D20DiceDock::ensureDiceSource(int result, qint64 nonce)
{
  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  if (!sceneSource) return false;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (!scene) { obs_source_release(sceneSource); return false; }

  char *pathRaw = obs_module_config_path((nonce % 2) ? "d20-roll-a.html" : "d20-roll-b.html");
  if (!pathRaw) { obs_source_release(sceneSource); return false; }
  const QString path = QString::fromUtf8(pathRaw);
  bfree(pathRaw);
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) { obs_source_release(sceneSource); return false; }
  file.write(htmlForRoll(result, nonce).toUtf8());
  file.close();

  obs_source_t *source = obs_get_source_by_name(DiceSourceName);
  if (!source) {
    obs_data_t *settings = obs_data_create();
    obs_data_set_bool(settings, "is_local_file", true);
    obs_data_set_string(settings, "local_file", path.toUtf8().constData());
    obs_data_set_int(settings, "width", 700);
    obs_data_set_int(settings, "height", 700);
    obs_data_set_int(settings, "fps", 60);
    source = obs_source_create("browser_source", DiceSourceName, settings, nullptr);
    obs_data_release(settings);
    if (!source) { obs_source_release(sceneSource); return false; }
  } else {
    obs_data_t *settings = obs_source_get_settings(source);
    obs_data_set_bool(settings, "is_local_file", true);
    obs_data_set_string(settings, "local_file", path.toUtf8().constData());
    obs_data_set_int(settings, "width", 700);
    obs_data_set_int(settings, "height", 700);
    obs_source_update(source, settings);
    obs_data_release(settings);
  }

  obs_sceneitem_t *item = obs_scene_find_source(scene, DiceSourceName);
  if (!item) item = obs_scene_add(scene, source);
  if (item) obs_sceneitem_set_visible(item, true);
  obs_source_release(source);
  obs_source_release(sceneSource);
  return item != nullptr;
}

void D20DiceDock::roll()
{
  const int result = QRandomGenerator::global()->bounded(1, 21);
  const qint64 nonce = QDateTime::currentMSecsSinceEpoch();
  if (ensureDiceSource(result, nonce)) {
    resultLabel->setText(QString("Result: %1").arg(result));
    rollButton->setText("Reroll D20");
  } else {
    resultLabel->setText("Could not create Browser Source");
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
