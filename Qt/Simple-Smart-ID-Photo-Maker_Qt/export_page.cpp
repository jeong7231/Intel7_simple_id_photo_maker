#include "export_page.h"
#include "ui_export_page.h"

export_page::export_page(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::export_page)
{
    ui->setupUi(this);
}

export_page::~export_page()
{
    delete ui;
}
