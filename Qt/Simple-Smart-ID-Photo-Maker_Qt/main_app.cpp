#include "main_app.h"
#include "ui_main_app.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QTimer>

main_app::main_app(QWidget *parent) : QWidget(parent), editPage(nullptr), exportPage(nullptr), ui(new Ui::main_app), timer(new QTimer(this))
{
    ui->setupUi(this);

    // 합성 엔진 초기화
    comp_.setCanvas(300, 400, 290);
    comp_.loadSuit("../../image/man_suit_bg_remove.png");
    comp_.loadGuide("../../image/man_suit_bg_remove.png");
    comp_.setMirror(true);
    comp_.setGuideVisible(true);
    comp_.setGuideOpacity(0.7);

    // 배경색 초기화 (기본 흰색)
    selectedBackgroundColor = cv::Scalar(255, 255, 255);
    comp_.setBackgroundColor(selectedBackgroundColor);

    // 카메라
    camera.open(0, cv::CAP_V4L2);
    if (!camera.isOpened())
        camera.open(0, cv::CAP_ANY);
    if (camera.isOpened())
    {
        camera.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    }

    // 타이머
    connect(timer, &QTimer::timeout, this, &main_app::updateFrame);

    // 촬영 버튼
    connect(ui->takePhotoButton, &QPushButton::clicked, this, &main_app::capturePhoto);

    if (camera.isOpened())
        timer->start(30);
}

void main_app::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    int newWidth = this->width();
    int newHeight = newWidth * 4 / 3;
    if (this->height() != newHeight) {
        this->setFixedHeight(newHeight);
    }
}

void main_app::updateFrame()
{
    if (!camera.isOpened())
        return;

    camera >> lastFrameBGR_;
    if (lastFrameBGR_.empty())
        return;

    // 수트 가이드를 포함한 프리뷰(BGR)
    cv::Mat prevBGR = comp_.makePreviewBGR(lastFrameBGR_);

    // Mat(BGR) -> QImage
    QImage qimg = SuitComposer::matBGR2QImage(prevBGR);

    ui->camScreen->setPixmap(QPixmap::fromImage(qimg));
    ui->camScreen->setScaledContents(true);
}

void main_app::capturePhoto()
{
    if (lastFrameBGR_.empty())
    {
        if (!camera.isOpened())
            return;
        camera >> lastFrameBGR_;
        if (lastFrameBGR_.empty())
            return;
    }

    // 수트 ⊕ 얼굴 합성 + 배경색 적용된 BGR 이미지
    cv::Mat outBGR = comp_.composeBGR(lastFrameBGR_);

    // 저장 경로
    QDir().mkpath("result");
    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    currentImagePath = QString("result/suit_%1.jpg").arg(ts); // JPG로 변경

    // JPG BGR 저장
    cv::imwrite(currentImagePath.toStdString(), outBGR, {cv::IMWRITE_JPEG_QUALITY, 95});

    // 편집 페이지로 이동
    if (!editPage)
    {
        editPage = new PhotoEditPage();
        editPage->setMainApp(this);
    }
    editPage->loadImage(currentImagePath);
    editPage->show();
    this->hide();
}

void main_app::goToExportPage()
{
    if (!exportPage)
        exportPage = new export_page();
    exportPage->show();
    editPage->hide();
    this->hide();
}

void main_app::goToExportPageWithImage()
{
    if (!exportPage)
    {
        exportPage = new export_page();
    }

    // PhotoEditPage에서 편집된 이미지를 가져와서 export_page에 설정
    if (editPage)
    {
        cv::Mat editedImage = editPage->getCurrentImage();
        if (!editedImage.empty())
        {
            exportPage->setResultImage(editedImage);
        }
        else
        {
        }
    }

    exportPage->show();
    editPage->hide();
    this->hide();
}

void main_app::on_colorSelect_currentTextChanged(const QString &text)
{
    // 선택된 색상에 따라 배경색 설정 (BGR 순서)
    if (text == "흰색")
    {
        selectedBackgroundColor = cv::Scalar(255, 255, 255);
    }
    else if (text == "파란색")
    {
        selectedBackgroundColor = cv::Scalar(255, 0, 0);
    }
    else if (text == "빨간색")
    {
        selectedBackgroundColor = cv::Scalar(0, 0, 255);
    }
    else if (text == "회색")
    {
        selectedBackgroundColor = cv::Scalar(128, 128, 128);
    }

    // SuitComposer에 배경색 적용
    comp_.setBackgroundColor(selectedBackgroundColor);
}

main_app::~main_app()
{
    camera.release();
    delete editPage;
    delete exportPage;
    delete ui;
}
