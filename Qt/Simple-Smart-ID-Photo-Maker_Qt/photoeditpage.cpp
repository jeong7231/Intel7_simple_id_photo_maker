#include "photoeditpage.h"
#include "ui_photoeditpage.h"
#include "main_app.h"
#include "QDateTime"

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
    connect(ui->BW_Button, &QPushButton::toggled, this, &PhotoEditPage::on_BW_Button_clicked);
}

PhotoEditPage::~PhotoEditPage()
{
    delete ui;
}

void PhotoEditPage::loadImage(const QString& path)
{
    originalImage = cv::imread(path.toStdString());
    currentImage = originalImage.clone();

    displayCurrentImage(currentImage);
}

cv::Mat PhotoEditPage::displayCurrentImage(cv::Mat& image)
{
    cv::Mat display_image;
    if (image.channels() == 3)
    {
        cv::cvtColor(image, display_image, cv::COLOR_BGR2RGB);
    }
    else if (image.channels() == 1)
    {
        cv::cvtColor(image, display_image, cv::COLOR_GRAY2RGB);
    }
    else
    {
        display_image = image.clone();
    }

    QImage qimg(display_image.data, display_image.cols, display_image.rows, display_image.step, QImage::Format_RGB888);
    QPixmap pixmap = QPixmap::fromImage(qimg);
    ui->photoScreen->setPixmap(pixmap);
    ui->photoScreen->setScaledContents(true);

    return display_image;
}

void PhotoEditPage::on_BW_Button_clicked(bool checked)
{
    isBWMode = checked;
    applyAllEffects();
}


void PhotoEditPage::applyAllEffects()
{
    currentImage = originalImage.clone();

    if (isBWMode) {
        cv::cvtColor(currentImage, currentImage, cv::COLOR_BGR2GRAY);
        cv::cvtColor(currentImage, currentImage, cv::COLOR_GRAY2BGR);
    }

    if (isHorizontalFlipped) {
        cv::flip(currentImage, currentImage, 1);
    }

    displayCurrentImage(currentImage);
}

void PhotoEditPage::on_horizontal_flip_button_clicked()
{
    isHorizontalFlipped = !isHorizontalFlipped;
    applyAllEffects();
}

