#ifndef GRAPHICSPAGE_H
#define GRAPHICSPAGE_H

#include <QWidget>
#include "ui_graphicspage.h"

class GraphicsPage : public QWidget, private Ui::GraphicsPage
{
    Q_OBJECT
public:
    explicit GraphicsPage(QWidget *parent = nullptr);

signals:

public slots:
};

#endif // GRAPHICSPAGE_H
