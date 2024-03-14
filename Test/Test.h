#pragma once

#include <QtWidgets/QWidget>
#include "CWindowPasteWidget.h"
#include "ui_Test.h"

class Test : public CWindowPasteWidget
{
    Q_OBJECT

public:
    Test(QWidget *parent = Q_NULLPTR);

private:
    Ui::TestClass ui;
};
