#include "trpg-status-dock.hpp"
#include <obs.h>
#include <obs-frontend-api.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <algorithm>

TRPGStatusDock::TRPGStatusDock(QWidget *parent) : QWidget(parent) {
  auto *root = new QVBoxLayout(this); auto *top = new QHBoxLayout;
  auto *add = new QPushButton("+ Character"); auto *refresh = new QPushButton("Create / Refresh HUD");
  top->addWidget(add); top->addWidget(refresh); root->addLayout(top);
  statusLabel = new QLabel("HP changes are saved and applied automatically."); root->addWidget(statusLabel);
  listLayout = new QVBoxLayout; root->addLayout(listLayout); root->addStretch();
  connect(add,&QPushButton::clicked,this,[this]{addCharacter();}); connect(refresh,&QPushButton::clicked,this,[this]{updateOverlay();});
  load(); rebuild(); updateOverlay();
}
QString TRPGStatusDock::configPath() const { QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/trpg-status"; QDir().mkpath(dir); return dir+"/characters.json"; }
void TRPGStatusDock::load(){ QFile f(configPath()); if(!f.open(QIODevice::ReadOnly))return; auto doc=QJsonDocument::fromJson(f.readAll()); for(const auto &v:doc.array()){auto o=v.toObject();Character c;c.name=o["name"].toString();c.maxHp=std::max(1,o["maxHp"].toInt(30));c.hp=std::clamp(o["hp"].toInt(c.maxHp),0,c.maxHp);if(!c.name.isEmpty())chars.push_back(c);} }
void TRPGStatusDock::save(){QJsonArray a;for(const auto &c:chars){QJsonObject o;o["name"]=c.name;o["hp"]=c.hp;o["maxHp"]=c.maxHp;a.append(o);}QFile f(configPath());if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(a).toJson());}
void TRPGStatusDock::addCharacter(){bool ok=false;QString name=QInputDialog::getText(this,"Add Character","Name:",QLineEdit::Normal,"",&ok);if(!ok||name.trimmed().isEmpty())return;int max=QInputDialog::getInt(this,"Add Character","Max HP:",30,1,99999,1,&ok);if(!ok)return;chars.push_back({name.trimmed(),max,max});save();rebuild();updateOverlay();}
void TRPGStatusDock::removeCharacter(int i){if(i<0||i>=chars.size())return;chars.removeAt(i);save();rebuild();updateOverlay();}
void TRPGStatusDock::changeHp(int i,int d){if(i<0||i>=chars.size())return;chars[i].hp=std::clamp(chars[i].hp+d,0,chars[i].maxHp);save();rebuild();updateOverlay();}
void TRPGStatusDock::setHp(int i){if(i<0||i>=chars.size())return;bool ok=false;int v=QInputDialog::getInt(this,"Set HP",chars[i].name+" HP:",chars[i].hp,0,chars[i].maxHp,1,&ok);if(ok){chars[i].hp=v;save();rebuild();updateOverlay();}}
void TRPGStatusDock::rebuild(){while(auto *item=listLayout->takeAt(0)){if(item->widget())delete item->widget();delete item;}for(int i=0;i<chars.size();++i){auto *row=new QWidget;auto *h=new QHBoxLayout(row);h->setContentsMargins(0,2,0,2);auto *name=new QLabel(chars[i].name);name->setMinimumWidth(90);h->addWidget(name);auto *hp=new QLabel(QString("%1 / %2").arg(chars[i].hp).arg(chars[i].maxHp));hp->setMinimumWidth(70);h->addWidget(hp);for(int d:{-5,-1,1,5}){auto *b=new QPushButton(d>0?QString("+%1").arg(d):QString::number(d));connect(b,&QPushButton::clicked,this,[this,i,d]{changeHp(i,d);});h->addWidget(b);}auto *set=new QPushButton("Set");connect(set,&QPushButton::clicked,this,[this,i]{setHp(i);});h->addWidget(set);auto *del=new QPushButton("x");connect(del,&QPushButton::clicked,this,[this,i]{removeCharacter(i);});h->addWidget(del);listLayout->addWidget(row);}}
QString TRPGStatusDock::makeHtml() const {
  QString cards;
  for(const auto &c:chars){
    int pct=c.maxHp?c.hp*100/c.maxHp:0;
    QString color=pct>=60?"#39c86a":(pct>=30?"#f29a2e":"#e94343");
    cards+=QString("<div class='card'><div class='top'><b>%1</b><span>%2 / %3</span></div><div class='track'><div class='bar' style='width:%4%;background-color:%5'></div></div></div>")
      .arg(c.name.toHtmlEscaped()).arg(c.hp).arg(c.maxHp).arg(pct).arg(color);
  }
  return QString(R"HTML(<!doctype html><html><head><meta charset='utf-8'><style>html,body{margin:0;background:transparent;font-family:Arial,sans-serif;color:white}.wrap{padding:24px;width:720px;box-sizing:border-box}.card{margin:0 0 14px;padding:14px 16px;background:rgba(12,15,24,.88);border:2px solid rgba(255,255,255,.18);border-radius:14px}.top{display:flex;justify-content:space-between;font-size:24px;margin-bottom:9px}.track{height:24px;background:rgba(255,255,255,.12);border-radius:12px;overflow:hidden}.bar{height:100%;min-width:0;transition:width .45s ease,background-color .25s ease}</style></head><body><div class='wrap'>%1</div></body></html>)HTML").arg(cards);
}
void TRPGStatusDock::updateOverlay(){
  QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/trpg-status";QDir().mkpath(dir);
  const qint64 nonce=QDateTime::currentMSecsSinceEpoch();
  const QString newPath=dir+QString("/hud-%1.html").arg(nonce);
  QFile f(newPath);if(f.open(QIODevice::WriteOnly|QIODevice::Truncate)){f.write(makeHtml().toUtf8());f.close();}
  const QString oldPath=htmlPath; htmlPath=newPath;
  obs_source_t *source=obs_get_source_by_name("TRPG Status HUD");
  obs_data_t *settings=obs_data_create();obs_data_set_bool(settings,"is_local_file",true);obs_data_set_string(settings,"local_file",htmlPath.toUtf8().constData());obs_data_set_int(settings,"width",768);obs_data_set_int(settings,"height",900);obs_data_set_int(settings,"fps",60);
  if(source){obs_source_update(source,settings);obs_source_media_restart(source);}else{source=obs_source_create("browser_source","TRPG Status HUD",settings,nullptr);if(source){obs_source_t *sceneSource=obs_frontend_get_current_scene();obs_scene_t *scene=sceneSource?obs_scene_from_source(sceneSource):nullptr;if(scene)obs_scene_add(scene,source);if(sceneSource)obs_source_release(sceneSource);}}
  obs_data_release(settings);if(source)obs_source_release(source);
  if(!oldPath.isEmpty()&&oldPath!=htmlPath)QFile::remove(oldPath);
  statusLabel->setText("HUD updated automatically.");
}
