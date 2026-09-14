#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>

class D20DiceDock : public QWidget {
  Q_OBJECT
public:
  explicit D20DiceDock(QWidget *parent = nullptr);

private:
  QLabel *resultLabel{};
  QLabel *totalLabel{};
  QComboBox *dieTypeCombo{};
  QPushButton *addDieButton{};
  QPushButton *rollButton{};
  QPushButton *visibilityButton{};
  QVBoxLayout *diceListLayout{};
  QWidget *diceListWidget{};
  QVector<int> diceSides;
  QVector<int> lastResults;
  QString currentHtmlPath;

  void addDie(int sides);
  void removeDie(int index);
  void rebuildDiceList();
  void roll();
  void toggleVisibility();
  void updateVisibilityButton();
  void saveDiceConfig();
  void loadDiceConfig();
  QString configPath() const;
  bool ensureDiceSource(const QVector<int> &results, qint64 nonce);
};
