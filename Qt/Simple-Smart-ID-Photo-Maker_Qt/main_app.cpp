#include "main_app.h"
#include "ui_main_app.h"
#include <QPixmap>
#include <QImage>
#include <QTimer>
#include <QDateTime>
#include <QDebug>

main_app::main_app(QWidget *parent)
    : QWidget(parent)
    , editPage(nullptr)
    , exportPage(nullptr)
    , ui(new Ui::main_app)
    , timer(new QTimer(this))
{
    ui->setupUi(this);

    // 카메라 초기화
    camera.open(0);

    // 타이머 연결
    connect(timer, &QTimer::timeout, this, &main_app::updateFrame);

    // 촬영 버튼 연결
    connect(ui->takePhotoButton, &QPushButton::clicked, this, &main_app::capturePhoto);

    // 카메라 시작
    if(camera.isOpened()) {
        timer->start(30); // 30ms마다 프레임 업데이트
    }
}

void main_app::updateFrame()
{
    cv::Mat frame;
    camera >> frame;

    if(!frame.empty()) {
        // BGR을 RGB로 변환
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

        // Mat을 QImage로 변환
        QImage qimg(frame.data, frame.cols, frame.rows, frame.step,
                    QImage::Format_RGB888);

        // QLabel에 표시
        ui->camScreen->setPixmap(QPixmap::fromImage(qimg));
        ui->camScreen->setScaledContents(true);
    }
}

void main_app::capturePhoto()
{
    cv::Mat frame;
    camera >> frame;

    if(!frame.empty()) {
        //현재 날짜, 시간으로 파일명
        QString timestamp =
            QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
        currentImagePath = QString("%1.jpg").arg(timestamp);

        // 사진 저장
        bool saved = cv::imwrite(currentImagePath.toStdString(), frame);

        if(!saved) {
            qDebug() << "Failed to save image to:" << currentImagePath;
            return;
        }

        // photoEditPage로 이동
        if(!editPage) {
            editPage = new PhotoEditPage();
            editPage->setMainApp(this);
        }
        editPage->loadImage(currentImagePath);
        editPage->show();
        this->hide();
    }
}

void main_app::goToExportPage()
{
    if(!exportPage)
    {
        exportPage = new export_page();
    }

    exportPage->show();
    editPage->hide();
    this->hide();
}

void main_app::goToExportPageWithImage()
{
    if(!exportPage)
    {
        exportPage = new export_page();
    }

    // PhotoEditPage에서 편집된 이미지를 가져와서 export_page에 설정
    if(editPage) {
        cv::Mat editedImage = editPage->getCurrentImage();
        if(!editedImage.empty()) {
            exportPage->setResultImage(editedImage);
            qDebug() << "Transferred edited image to export page";
        } else {
            qDebug() << "No edited image available to transfer";
        }
    }

    exportPage->show();
    editPage->hide();
    this->hide();
}

main_app::~main_app()
{
    camera.release();
    if(editPage)
        delete editPage;
    if(exportPage)
        delete exportPage;
    delete ui;
}
