#include "main_app.h"
#include "ui_main_app.h"
#include <QPixmap>
#include <QImage>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QDir>

main_app::main_app(QWidget *parent)
    : QWidget(parent)
      , editPage(nullptr)
      , exportPage(nullptr)
      , ui(new Ui::main_app)
      , timer(new QTimer(this))
{
    ui->setupUi(this);

    // 합성 엔진 초기화
    comp_.setCanvas(300, 400, 290);
    comp_.loadSuit("../../image/man_suit_bg_remove.png");
    comp_.loadGuide("../../image/man_suit_bg_remove.png");
    comp_.setMirror(true);
    comp_.setGuideVisible(true);
    comp_.setGuideOpacity(0.7);

    // 카메라
    camera.open(0, cv::CAP_V4L2);
    if(!camera.isOpened()) camera.open(0, cv::CAP_ANY);
    if(camera.isOpened()){
        camera.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
        camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    }

    // 타이머
    connect(timer, &QTimer::timeout, this, &main_app::updateFrame);

    // 촬영 버튼
    connect(ui->takePhotoButton, &QPushButton::clicked, this, &main_app::capturePhoto);

    if(camera.isOpened()) timer->start(30);
}

void main_app::updateFrame()
{
    if(!camera.isOpened()) return;

    camera >> lastFrameBGR_;
    if(lastFrameBGR_.empty()) return;

    // 수트 가이드를 포함한 프리뷰(BGR)
    cv::Mat prevBGR = comp_.makePreviewBGR(lastFrameBGR_);

    // Mat(BGR) -> QImage
    QImage qimg = SuitComposer::matBGR2QImage(prevBGR);

    ui->camScreen->setPixmap(QPixmap::fromImage(qimg));
    ui->camScreen->setScaledContents(true);
}

void main_app::capturePhoto()
{
    if(lastFrameBGR_.empty()){
        if(!camera.isOpened()) return;
        camera >> lastFrameBGR_;
        if(lastFrameBGR_.empty()) return;
    }

    // 수트 ⊕ 얼굴 합성 RGBA
    cv::Mat outRGBA = comp_.composeRGBA(lastFrameBGR_);

    // 저장 경로
    QDir().mkpath("result");
    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    currentImagePath = QString("result/suit_%1.png").arg(ts);

    // PNG RGBA 저장
    cv::imwrite(currentImagePath.toStdString(), outRGBA, {cv::IMWRITE_PNG_COMPRESSION, 3});

    // 편집 페이지로 이동
    if(!editPage) {
        editPage = new PhotoEditPage();
        editPage->setMainApp(this);
    }
    editPage->loadImage(currentImagePath);
    editPage->show();
    this->hide();
}

void main_app::goToExportPage()
{
    if(!exportPage) exportPage = new export_page();
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
    delete editPage;
    delete exportPage;
    delete ui;
}
