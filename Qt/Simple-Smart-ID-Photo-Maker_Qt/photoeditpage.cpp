#include "photoeditpage.h"
#include "ui_photoeditpage.h"
#include "main_app.h"
#include "QDateTime"
#include <algorithm>
#include <QDebug>
#include <QStringList>

PhotoEditPage::PhotoEditPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PhotoEditPage)
    , mainApp(nullptr)
{
    ui->setupUi(this);

    // 선명도 트랙바 설정
    ui->Sharpen_bar->setRange(0, 10);
    ui->Sharpen_bar->setValue(0);

    // 눈 크기 트랙바 설정
    ui->eye_size_bar->setRange(0, 10);
    ui->eye_size_bar->setValue(0);

    // 캐스케이드 분류기 로드 (절대경로)
    if (!faceCascade.load("/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_default.xml")) {
        qDebug() << "Failed to load face cascade classifier";
    }

    // 모든 눈 캐스케이드 파일을 순차적으로 시도
    QStringList eyeCascadePaths = {
        "/usr/local/share/opencv4/haarcascades/haarcascade_eye.xml",
        "/usr/local/share/opencv4/haarcascades/haarcascade_eye_tree_eyeglasses.xml",
        "/usr/local/share/opencv4/haarcascades/haarcascade_lefteye_2splits.xml",
        "/usr/local/share/opencv4/haarcascades/haarcascade_righteye_2splits.xml"
    };

    bool eyeCascadeLoaded = false;
    for (const QString& path : eyeCascadePaths) {
        if (eyeCascade.load(path.toStdString())) {
            qDebug() << "Successfully loaded eye cascade:" << path;
            eyeCascadeLoaded = true;
            break;
        } else {
            qDebug() << "Failed to load eye cascade:" << path;
        }
    }

    if (!eyeCascadeLoaded) {
        qDebug() << "Failed to load any eye cascade classifier";
    }
}

void PhotoEditPage::setMainApp(main_app* app)
{
    mainApp = app;
    connect(ui->finish_button, &QPushButton::clicked, mainApp, &main_app::goToExportPageWithImage);
    connect(ui->BW_Button, &QPushButton::toggled, this, &PhotoEditPage::on_BW_Button_clicked);
}

PhotoEditPage::~PhotoEditPage()
{
    delete ui;
}

void PhotoEditPage::loadImage(const QString& path)
{
    originalImage = cv::imread(path.toStdString());

    if(originalImage.empty()) {
        qDebug() << "Failed to load image from:" << path;
        return;
    }

    currentImage = originalImage.clone();
    displayCurrentImage(currentImage);
}

cv::Mat PhotoEditPage::displayCurrentImage(cv::Mat& image)
{
    if (image.empty()) {
        qDebug() << "Cannot display empty image";
        return cv::Mat();
    }

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
    if (originalImage.empty()) {
        qDebug() << "Cannot apply effects: original image is empty";
        return;
    }

    currentImage = originalImage.clone();

    // 선명도 조정 적용
    if (sharpnessStrength > 0) {
        sharpen(currentImage, sharpnessStrength);
    }

    // 눈 크기 조정 적용 (얼굴 검출 필요)
    if (eyeSizeStrength > 0) {
        if (faceCascade.empty()) {
            qDebug() << "Face cascade not loaded, eye size adjustment disabled";
        } else {
            std::vector<cv::Rect> faces;
            cv::Mat gray;
            cv::cvtColor(currentImage, gray, cv::COLOR_BGR2GRAY);
            cv::equalizeHist(gray, gray);
            faceCascade.detectMultiScale(gray, faces, 1.1, 5, 0, cv::Size(80, 80));

            if (!faces.empty()) {
                qDebug() << "Detected" << faces.size() << "faces, applying eye correction";
                cv::Rect face_roi = safeRect(faces[0].x, faces[0].y, faces[0].width, faces[0].height,
                                             currentImage.cols, currentImage.rows);
                if (!face_roi.empty()) {
                    correctEyes(currentImage, face_roi, eyeSizeStrength);
                }
            } else {
                qDebug() << "No faces detected for eye size adjustment";
            }
        }
    }

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


// opencv_origin.cpp 기반 선명도 조정 함수
void PhotoEditPage::sharpen(cv::Mat& image, int strength) {
    if (strength <= 0 || image.empty()) return;
    if (image.cols <= 0 || image.rows <= 0 || image.cols > 3000 || image.rows > 3000) return;

    cv::Size original_size = image.size();
    const int UPSCALE_LIMIT = 1500;

    cv::Mat upscaled;
    double upscale_factor = 2.0;
    cv::Size new_size(original_size.width * upscale_factor, original_size.height * upscale_factor);

    if (new_size.width > UPSCALE_LIMIT || new_size.height > UPSCALE_LIMIT) {
        upscale_factor = (double)UPSCALE_LIMIT / std::max(original_size.width, original_size.height);
        new_size = cv::Size(original_size.width * upscale_factor, original_size.height * upscale_factor);
    }

    if (new_size.width <= 0 || new_size.height <= 0) return;

    cv::resize(image, upscaled, new_size, 0, 0, cv::INTER_CUBIC);

    cv::Mat blurred;
    cv::GaussianBlur(upscaled, blurred, cv::Size(0, 0), 3);

    float amount = strength / 10.0f;
    cv::addWeighted(upscaled, 1.0f + amount, blurred, -amount, 0, upscaled);

    cv::resize(upscaled, image, original_size, 0, 0, cv::INTER_AREA);
}

void PhotoEditPage::on_Sharpen_bar_actionTriggered(int)
{
    sharpnessStrength = ui->Sharpen_bar->value();
    applyAllEffects();
}




void PhotoEditPage::on_eye_size_bar_valueChanged(int value)
{
    eyeSizeStrength = value;
    applyAllEffects();
}

cv::Mat PhotoEditPage::getCurrentImage() const
{
    return currentImage;
}

cv::Rect PhotoEditPage::safeRect(int x, int y, int w, int h, int maxW, int maxH) {
    if (w <= 0 || h <= 0) return cv::Rect();
    cv::Rect r(x, y, w, h);
    cv::Rect bounds(0, 0, maxW, maxH);
    r &= bounds;
    if (r.width <= 0 || r.height <= 0) return cv::Rect();
    return r;
}

void PhotoEditPage::correctEyes(cv::Mat& image, cv::Rect roi, int strength) {
    if (strength <= 0 || image.empty()) return;

    if (eyeCascade.empty()) {
        qDebug() << "Eye cascade not loaded, cannot perform eye correction";
        return;
    }

    float enlargement_factor = strength / 20.0f;
    if (enlargement_factor <= 0) return;

    cv::Rect safe_face = safeRect(roi.x, roi.y, roi.width, roi.height, image.cols, image.rows);
    if (safe_face.empty()) return;

    cv::Rect upper_face_roi(safe_face.x, safe_face.y, safe_face.width, std::max(1, safe_face.height * 2 / 3));
    if (upper_face_roi.width <= 0 || upper_face_roi.height <= 0) return;

    cv::Mat roi_img = image(upper_face_roi);
    if (roi_img.empty()) return;

    std::vector<cv::Rect> eyes;
    cv::Mat gray;
    cv::cvtColor(roi_img, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);
    // 더 완화된 눈 검출 파라미터 시도
    eyeCascade.detectMultiScale(gray, eyes, 1.05, 2, 0, cv::Size(10, 10), cv::Size(100, 100));

    qDebug() << "Detected" << eyes.size() << "eyes in face region";

    // 눈이 검출되지 않으면 가상의 눈 위치를 추정
    if (eyes.size() < 1) {
        qDebug() << "No eyes detected, estimating eye positions";
        int face_width = upper_face_roi.width;
        int face_height = upper_face_roi.height;

        // 얼굴의 상단 1/3 지점에서 좌우 1/4 지점에 눈 위치 추정
        int eye_y = face_height / 3;
        int left_eye_x = face_width / 4;
        int right_eye_x = face_width * 3 / 4;
        int eye_size = std::min(face_width / 8, face_height / 6);

        cv::Rect left_eye(left_eye_x - eye_size/2, eye_y - eye_size/2, eye_size, eye_size);
        cv::Rect right_eye(right_eye_x - eye_size/2, eye_y - eye_size/2, eye_size, eye_size);

        eyes.clear();
        eyes.push_back(left_eye);
        eyes.push_back(right_eye);

        qDebug() << "Estimated eye positions: left(" << left_eye.x << "," << left_eye.y << ") right(" << right_eye.x << "," << right_eye.y << ")";
    }

    // 2개 이상의 눈이 검출된 경우 가장 큰 2개만 사용
    if (eyes.size() > 2) {
        std::sort(eyes.begin(), eyes.end(), [](const cv::Rect& a, const cv::Rect& b) {
            return a.area() > b.area();
        });
        eyes.resize(2);
    }

    std::sort(eyes.begin(), eyes.end(), [](const cv::Rect& a, const cv::Rect& b) {
        return a.x < b.x;
    });

    cv::Mat original_roi_img = roi_img.clone();

    for (const auto& eye_roi_raw : eyes) {
        cv::Rect eye_roi = safeRect(eye_roi_raw.x, eye_roi_raw.y, eye_roi_raw.width, eye_roi_raw.height,
                                    roi_img.cols, roi_img.rows);
        if (eye_roi.empty()) continue;

        int side = std::min(eye_roi.width, eye_roi.height);
        cv::Point center(eye_roi.x + eye_roi.width / 2, eye_roi.y + eye_roi.height / 2);
        cv::Rect square_eye_roi(center.x - side / 2, center.y - side / 2, side, side);
        square_eye_roi &= cv::Rect(0, 0, roi_img.cols, roi_img.rows);
        if (square_eye_roi.empty()) continue;

        cv::Mat original_eye_area = original_roi_img(square_eye_roi);
        if (original_eye_area.empty()) continue;

        float scale = 1.0f + enlargement_factor;
        int new_size = static_cast<int>(side * scale);
        qDebug() << "Eye enlargement: factor=" << enlargement_factor << "scale=" << scale << "original_size=" << side << "new_size=" << new_size;

        if (new_size <= side) {
            qDebug() << "Skipping eye: new_size" << new_size << "<= original_size" << side;
            continue;
        }

        cv::Mat enlarged_eye;
        cv::resize(original_eye_area, enlarged_eye, cv::Size(new_size, new_size), 0, 0, cv::INTER_CUBIC);
        qDebug() << "Successfully enlarged eye from" << side << "to" << new_size;

        cv::Rect target_roi(center.x - new_size / 2, center.y - new_size / 2, new_size, new_size);
        target_roi &= cv::Rect(0, 0, roi_img.cols, roi_img.rows);
        if (target_roi.empty()) continue;

        cv::Mat mask = cv::Mat::zeros(enlarged_eye.size(), CV_8UC1);
        cv::circle(mask, cv::Point(mask.cols / 2, mask.rows / 2), mask.cols / 2, cv::Scalar(255), -1);
        cv::GaussianBlur(mask, mask, cv::Size(21, 21), 10);

        cv::resize(enlarged_eye, enlarged_eye, target_roi.size());
        cv::resize(mask, mask, target_roi.size());

        enlarged_eye.copyTo(roi_img(target_roi), mask);
    }
}

