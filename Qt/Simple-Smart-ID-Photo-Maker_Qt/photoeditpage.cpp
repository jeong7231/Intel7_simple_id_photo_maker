#include "photoeditpage.h"
#include "ui_photoeditpage.h"
#include "main_app.h"

PhotoEditPage::PhotoEditPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PhotoEditPage)
    , mainApp(nullptr)
{
    ui->setupUi(this);

}

void PhotoEditPage::setMainApp(main_app* app)
{
    mainApp = app;
    connect(ui->finish_button, &QPushButton::clicked, mainApp, &main_app::goToExportPage);
}

PhotoEditPage::~PhotoEditPage()
{
    delete ui;
}

void PhotoEditPage::loadImage(const QString& imagePath)
{
    QPixmap pixmap(imagePath);
    ui->photoScreen->setPixmap(pixmap);
    ui->photoScreen->setScaledContents(true);
}
