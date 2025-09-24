#include "export_page.h"
#include "ui_export_page.h"
#include <QPixmap>
#include <QImage>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QFile>

export_page::export_page(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::export_page)
    , selectedFormat("jpg")
{
    ui->setupUi(this);

    // 기본 확장자 설정
    if (ui->file_format_select_combo->count() > 0) {
        selectedFormat = ui->file_format_select_combo->currentText().toLower();
    }
}

export_page::~export_page()
{
    delete ui;
}

void export_page::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    int newWidth = this->width();
    int newHeight = newWidth * 4 / 3;
    if (this->height() != newHeight) {
        this->setFixedHeight(newHeight);
    }
}

void export_page::setResultImage(const cv::Mat& image)
{
    if (image.empty()) {
        qDebug() << "Cannot set empty image to result screen";
        return;
    }

    resultImage = image.clone();

    // OpenCV Mat을 QImage로 변환
    cv::Mat display_image;
    if (image.channels() == 3) {
        cv::cvtColor(image, display_image, cv::COLOR_BGR2RGB);
    } else if (image.channels() == 1) {
        cv::cvtColor(image, display_image, cv::COLOR_GRAY2RGB);
    } else {
        display_image = image.clone();
    }

    QImage qimg(display_image.data, display_image.cols, display_image.rows, display_image.step, QImage::Format_RGB888);
    QPixmap pixmap = QPixmap::fromImage(qimg);

    // resultPhotoScreen에 이미지 표시
    ui->resultPhotoScreen->setPixmap(pixmap);
    ui->resultPhotoScreen->setScaledContents(true);

    qDebug() << "Result image set to export page with size:" << image.cols << "x" << image.rows;
}

void export_page::on_file_format_select_combo_currentTextChanged(const QString &text)
{
    selectedFormat = text.toLower();
    qDebug() << "Selected file format:" << selectedFormat;
}

QString export_page::generateUniqueFileName(const QString& baseName, const QString& extension)
{
    QString currentDir = QDir::currentPath();
    QString fileName = QString("%1%2").arg(baseName, extension);
    QString fullPath = currentDir + "/" + fileName;

    // 파일이 존재하지 않으면 원래 이름 사용
    if (!QFile::exists(fullPath)) {
        return fileName;
    }

    // 파일이 존재하면 번호를 추가하여 중복 방지
    int counter = 1;
    do {
        fileName = QString("%1%2%3").arg(baseName).arg(counter).arg(extension);
        fullPath = currentDir + "/" + fileName;
        counter++;
    } while (QFile::exists(fullPath));

    return fileName;
}

void export_page::on_export_button_clicked()
{
    if (resultImage.empty()) {
        QMessageBox::warning(this, "경고", "저장할 이미지가 없습니다.");
        qDebug() << "No image to export";
        return;
    }

    // 중복되지 않는 파일명 생성
    QString fileName = generateUniqueFileName("result", selectedFormat);
    QString filePath = QDir::currentPath() + "/" + fileName;

    qDebug() << "Attempting to save image as:" << filePath;
    qDebug() << "Image format:" << selectedFormat;
    qDebug() << "Image size:" << resultImage.cols << "x" << resultImage.rows;

    // OpenCV로 이미지 저장
    bool success = cv::imwrite(filePath.toStdString(), resultImage);

    if (success) {
        QMessageBox::information(this, "저장 완료",
            QString("이미지가 성공적으로 저장되었습니다:\n%1").arg(filePath));
        qDebug() << "Image saved successfully:" << filePath;
    } else {
        QMessageBox::critical(this, "저장 실패",
            QString("이미지 저장에 실패했습니다:\n%1").arg(filePath));
        qDebug() << "Failed to save image:" << filePath;
    }
}
