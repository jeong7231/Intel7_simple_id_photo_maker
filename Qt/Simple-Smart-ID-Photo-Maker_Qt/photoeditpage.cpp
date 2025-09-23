#include "photoeditpage.h"
#include "ui_photoeditpage.h"
#include "main_app.h"
#include "QDateTime"
#include <algorithm>
#include <QDebug>
#include <QStringList>

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

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

    // 배경색 초기화 (기본 흰색)
    createBackgroundWithColor(currentBackgroundColor);

    // 초기에 배경만 표시
    cv::Mat emptyMat;
    displayCurrentImage(emptyMat);

    // 캐스케이드 분류기 로드 (절대경로) - 시스템에서 찾은 절대경로 사용
    if (!faceCascade.load("/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_default.xml")) {
        qDebug() << "Failed to load face cascade classifier from system path";
        // 백업 경로도 시도
        if (!faceCascade.load("/home/ubuntu/opencv/Intel7_simple_id_photo_maker/jinsu/haarcascade_frontalface_default.xml")) {
            qDebug() << "Failed to load face cascade classifier from both paths";
        } else {
            qDebug() << "Successfully loaded face cascade from backup path";
        }
    } else {
        qDebug() << "Successfully loaded face cascade from system path";
    }

    // 모든 눈 캐스케이드 파일을 순차적으로 시도 (시스템 경로 우선)
    QStringList eyeCascadePaths = {
        "/usr/local/share/opencv4/haarcascades/haarcascade_eye.xml",
        "/usr/local/share/opencv4/haarcascades/haarcascade_eye_tree_eyeglasses.xml",
        "/usr/local/share/opencv4/haarcascades/haarcascade_lefteye_2splits.xml",
        "/usr/local/share/opencv4/haarcascades/haarcascade_righteye_2splits.xml",
        "/home/ubuntu/opencv/Intel7_simple_id_photo_maker/jinsu/haarcascade_eye_tree_eyeglasses.xml"
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

    // 얼굴 랜드마크 모델 초기화
    facemark = cv::face::FacemarkLBF::create();

    // 랜드마크 모델 파일 로드 (치아 미백에 필요) - 다운로드한 절대경로 사용
    try {
        facemark->loadModel("/tmp/lbfmodel.yaml");
        qDebug() << "Successfully loaded lbfmodel.yaml for facial landmarks";
    } catch (const cv::Exception& e) {
        qDebug() << "Failed to load lbfmodel.yaml from /tmp, trying backup path";
        try {
            facemark->loadModel("/home/ubuntu/opencv/Intel7_simple_id_photo_maker/jinsu/lbfmodel.yaml");
            qDebug() << "Successfully loaded lbfmodel.yaml from backup path";
        } catch (const cv::Exception& e2) {
            qDebug() << "Failed to load lbfmodel.yaml - teeth whitening may not work properly";
        }
    }
}

PhotoEditPage::~PhotoEditPage()
{
    delete ui;
}

void PhotoEditPage::setMainApp(main_app* app)
{
    mainApp = app;
    connect(ui->finish_button, &QPushButton::clicked, mainApp, &main_app::goToExportPageWithImage);
    connect(ui->BW_Button, &QPushButton::toggled, this, &PhotoEditPage::on_BW_Button_clicked);
    connect(ui->teeth_whiten_4_button, &QPushButton::toggled, this, &PhotoEditPage::on_teeth_whiten_4_button_clicked);
    connect(ui->retakeshot_button, &QPushButton::clicked, this, &PhotoEditPage::on_retakeshot_button_clicked);
    connect(ui->init_button, &QPushButton::clicked, this, &PhotoEditPage::on_init_button_clicked);
}

// ============================================================================
// IMAGE LOADING & DISPLAY
// ============================================================================

void PhotoEditPage::loadImage(const QString& path)
{
    originalImage = cv::imread(path.toStdString());

    if(originalImage.empty()) {
        qDebug() << "Failed to load image from:" << path;
        return;
    }

    currentImage = originalImage.clone();
    spotSmoothImage = originalImage.clone();
    displayCurrentImage(currentImage);
}

cv::Mat PhotoEditPage::displayCurrentImage(cv::Mat& image)
{
    cv::Mat display_image;

    // 배경 이미지가 있는 경우 항상 오버레이 적용
    if (!backgroundImage.empty()) {
        qDebug() << "Background image exists: " << backgroundImage.cols << "x" << backgroundImage.rows;

        if (image.empty()) {
            // 이미지가 없으면 배경만 표시
            display_image = backgroundImage.clone();
            qDebug() << "Displaying background only (no photo loaded)";
        } else {
            // 이미지가 있으면 배경 위에 오버레이
            display_image = overlayPhotoOnBackground(image, backgroundImage);
            qDebug() << "Displaying photo overlaid on RGB background";
        }

        // BGR에서 RGB로 변환
        cv::cvtColor(display_image, display_image, cv::COLOR_BGR2RGB);
        qDebug() << "Final display image size: " << display_image.cols << "x" << display_image.rows;
    } else {
        // 배경이 없는 경우 기존 로직
        qDebug() << "No background image available";
        if (image.empty()) {
            qDebug() << "Cannot display empty image and no background";
            return cv::Mat();
        }

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
    }

    QImage qimg(display_image.data, display_image.cols, display_image.rows, display_image.step, QImage::Format_RGB888);
    QPixmap pixmap = QPixmap::fromImage(qimg);
    ui->photoScreen->setPixmap(pixmap);
    ui->photoScreen->setScaledContents(true);

    return display_image;
}

cv::Mat PhotoEditPage::getCurrentImage() const
{
    return currentImage;
}

// ============================================================================
// EFFECT APPLICATION
// ============================================================================

void PhotoEditPage::applyAllEffects()
{
    if (originalImage.empty()) {
        qDebug() << "Cannot apply effects: original image is empty";
        return;
    }

    currentImage = spotSmoothImage.clone();

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

    // 치아 미백은 이제 수동으로만 적용 (마우스 클릭 시)

    if (isBWMode) {
        cv::cvtColor(currentImage, currentImage, cv::COLOR_BGR2GRAY);
        cv::cvtColor(currentImage, currentImage, cv::COLOR_GRAY2BGR);
    }

    if (isHorizontalFlipped) {
        cv::flip(currentImage, currentImage, 1);
    }

    displayCurrentImage(currentImage);
}

// ============================================================================
// UI EVENT HANDLERS
// ============================================================================

void PhotoEditPage::on_BW_Button_clicked(bool checked)
{
    isBWMode = checked;
    applyAllEffects();
}

void PhotoEditPage::on_horizontal_flip_button_clicked()
{
    isHorizontalFlipped = !isHorizontalFlipped;
    applyAllEffects();
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

void PhotoEditPage::on_spot_remove_pen_toggled(bool checked)
{
    isSpotRemovalMode = checked;
    if (checked) {
        // 치아 미백 모드 비활성화
        isTeethWhiteningMode = false;
        ui->teeth_whiten_4_button->setChecked(false);
        ui->photoScreen->setCursor(Qt::CrossCursor);
        qDebug() << "Spot removal mode enabled";
    } else {
        ui->photoScreen->setCursor(Qt::ArrowCursor);
        qDebug() << "Spot removal mode disabled";
    }
}

// ============================================================================
// IMAGE PROCESSING FUNCTIONS
// ============================================================================

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

// ============================================================================
// SPOT REMOVAL FUNCTIONS
// ============================================================================

void PhotoEditPage::applySmoothSpot(cv::Mat& image, const cv::Point& center, int radius) {
    if (image.empty()) return;
    if (image.cols <= 0 || image.rows <= 0 || image.cols > 5000 || image.rows > 5000) return;
    if (radius <= 0 || radius > 200) return;

    // 매우 작은 점 제거용 반지름 (최소 확대)
    int effectiveRadius = radius * 1.2;
    cv::Rect roi = safeRect(center.x - effectiveRadius, center.y - effectiveRadius,
                           2 * effectiveRadius, 2 * effectiveRadius, image.cols, image.rows);
    if (roi.empty()) return;

    if (roi.width > 400 || roi.height > 400) {
        qDebug() << "[applySmoothSpot] ROI too big:" << roi.width << "x" << roi.height;
        return;
    }

    cv::Mat roi_image = image(roi);
    if (roi_image.empty()) return;

    // 다중 단계 잡티 제거 알고리즘
    cv::Mat result = roi_image.clone();

    // 매우 작은 점 제거용 최소 처리
    cv::Mat smoothed1;
    cv::bilateralFilter(result, smoothed1, 5, 50, 50);

    // 최종 결과 (아주 자연스럽게)
    cv::Mat final_result;
    cv::addWeighted(result, 0.5, smoothed1, 0.5, 0, final_result);

    // 더 부드러운 마스크 생성
    cv::Mat mask = cv::Mat::zeros(roi.size(), CV_8UC1);
    cv::Point mask_center(roi.width / 2, roi.height / 2);
    int mask_radius = std::min(roi.width, roi.height) / 2;

    // 중심에서 가장자리로 갈수록 부드럽게 페이드되는 마스크
    for (int y = 0; y < mask.rows; y++) {
        for (int x = 0; x < mask.cols; x++) {
            double distance = cv::norm(cv::Point(x, y) - mask_center);
            double alpha = std::max(0.0, 1.0 - (distance / mask_radius));
            alpha = std::pow(alpha, 0.5); // 부드러운 커브
            mask.at<uchar>(y, x) = static_cast<uchar>(255 * alpha);
        }
    }

    // 매우 작은 점용 정밀 마스크
    cv::GaussianBlur(mask, mask, cv::Size(5, 5), 1.5);

    // 최종 적용
    final_result.copyTo(roi_image, mask);

    qDebug() << "Applied enhanced spot removal at (" << center.x << "," << center.y << ") with radius" << effectiveRadius;
}

void PhotoEditPage::applyInpaintSpot(cv::Mat& image, const cv::Point& center, int radius) {
    if (image.empty()) return;
    if (image.cols <= 0 || image.rows <= 0 || image.cols > 5000 || image.rows > 5000) return;
    if (radius <= 0 || radius > 200) return;

    // 매우 작은 점용 inpainting 영역
    int effectiveRadius = radius * 1.5;
    cv::Rect roi = safeRect(center.x - effectiveRadius, center.y - effectiveRadius,
                           2 * effectiveRadius, 2 * effectiveRadius, image.cols, image.rows);
    if (roi.empty()) return;

    cv::Mat roi_image = image(roi);
    if (roi_image.empty()) return;

    // inpainting용 마스크 생성
    cv::Mat inpaint_mask = cv::Mat::zeros(roi.size(), CV_8UC1);
    cv::Point mask_center(roi.width / 2, roi.height / 2);
    int mask_radius = radius;

    // 원형 마스크 생성
    cv::circle(inpaint_mask, mask_center, mask_radius, cv::Scalar(255), -1);

    // 작은 점용 inpainting (더 정밀하게)
    cv::Mat inpainted;
    cv::inpaint(roi_image, inpaint_mask, inpainted, 2, cv::INPAINT_TELEA);

    // 결과를 부드럽게 블렌딩
    cv::Mat blend_mask = cv::Mat::zeros(roi.size(), CV_8UC1);
    for (int y = 0; y < blend_mask.rows; y++) {
        for (int x = 0; x < blend_mask.cols; x++) {
            double distance = cv::norm(cv::Point(x, y) - mask_center);
            double alpha = std::max(0.0, 1.0 - (distance / (mask_radius * 1.5)));
            alpha = std::pow(alpha, 0.3); // 부드러운 블렌딩
            blend_mask.at<uchar>(y, x) = static_cast<uchar>(255 * alpha);
        }
    }

    cv::GaussianBlur(blend_mask, blend_mask, cv::Size(3, 3), 1);
    inpainted.copyTo(roi_image, blend_mask);

    qDebug() << "Applied inpaint spot removal at (" << center.x << "," << center.y << ") with radius" << effectiveRadius;
}

void PhotoEditPage::applyTeethWhitening(cv::Mat& image, const cv::Point& center, int radius) {
    if (image.empty()) return;
    if (image.cols <= 0 || image.rows <= 0 || image.cols > 5000 || image.rows > 5000) return;
    if (radius <= 0 || radius > 100) return;

    // 치아 미백 적용 영역 설정
    int effectiveRadius = radius;  // 치아 미백 크기 줄임
    cv::Rect roi = safeRect(center.x - effectiveRadius, center.y - effectiveRadius,
                           2 * effectiveRadius, 2 * effectiveRadius, image.cols, image.rows);
    if (roi.empty()) return;

    cv::Mat roi_image = image(roi);
    if (roi_image.empty()) return;

    // --- Lab 색공간 변환 ---
    cv::Mat lab_roi;
    cv::cvtColor(roi_image, lab_roi, cv::COLOR_BGR2Lab);

    // --- 원형 마스크 생성 ---
    cv::Mat mask = cv::Mat::zeros(roi.size(), CV_8UC1);
    cv::Point mask_center(roi.width / 2, roi.height / 2);

    // 그라데이션 마스크 생성 (중심에서 가장자리로 갈수록 약해짐)
    for (int y = 0; y < mask.rows; y++) {
        for (int x = 0; x < mask.cols; x++) {
            double distance = cv::norm(cv::Point(x, y) - mask_center);
            double alpha = std::max(0.0, 1.0 - (distance / effectiveRadius));
            alpha = std::pow(alpha, 0.5); // 부드러운 그라데이션
            mask.at<uchar>(y, x) = static_cast<uchar>(255 * alpha);
        }
    }

    // 마스크 부드럽게 처리
    cv::GaussianBlur(mask, mask, cv::Size(9, 9), 3);

    // --- 치아 미백 처리 ---
    float whitening_strength = 8.0f;  // 미백 강도
    float yellow_reduction = 6.0f;    // 노란기 제거 강도

    for (int r = 0; r < lab_roi.rows; ++r) {
        for (int c = 0; c < lab_roi.cols; ++c) {
            uchar mask_val = mask.at<uchar>(r, c);
            if (mask_val > 5) {  // 마스크 임계값
                cv::Vec3b& lab_pixel = lab_roi.at<cv::Vec3b>(r, c);
                float alpha = mask_val / 255.0f;  // 마스크 강도에 따른 알파 블렌딩

                // L 채널 (밝기) 대폭 증가
                int new_l = lab_pixel[0] + static_cast<int>(whitening_strength * alpha);
                lab_pixel[0] = cv::saturate_cast<uchar>(std::min(new_l, 255));

                // A 채널 (초록-빨강) 중성으로 조정
                int a_center = 128;
                int new_a = lab_pixel[1] + static_cast<int>((a_center - lab_pixel[1]) * alpha * 0.3f);
                lab_pixel[1] = cv::saturate_cast<uchar>(new_a);

                // B 채널 (파랑-노랑) 파랑쪽으로 강하게 조정 (노란기 제거)
                int new_b = lab_pixel[2] - static_cast<int>(yellow_reduction * alpha);
                lab_pixel[2] = cv::saturate_cast<uchar>(std::max(new_b, 0));
            }
        }
    }

    // --- BGR로 다시 변환 ---
    cv::Mat whitened_roi;
    cv::cvtColor(lab_roi, whitened_roi, cv::COLOR_Lab2BGR);

    // --- 원본 이미지에 적용 ---
    cv::Mat final_mask_3ch;
    cv::cvtColor(mask, final_mask_3ch, cv::COLOR_GRAY2BGR);
    final_mask_3ch.convertTo(final_mask_3ch, CV_32F, 1.0/255.0);

    cv::Mat roi_float, whitened_float, original_float;
    roi_image.convertTo(original_float, CV_32F);
    whitened_roi.convertTo(whitened_float, CV_32F);

    // 알파 블렌딩으로 자연스럽게 합성
    cv::Mat result_float;
    cv::multiply(final_mask_3ch, whitened_float, result_float);
    cv::multiply(cv::Scalar::all(1.0) - final_mask_3ch, original_float, original_float);
    cv::add(result_float, original_float, result_float);

    result_float.convertTo(roi_image, CV_8U);

    qDebug() << "Applied manual teeth whitening at (" << center.x << "," << center.y << ") with radius" << effectiveRadius;
}

// ============================================================================
// MOUSE EVENT HANDLERS
// ============================================================================

void PhotoEditPage::mousePressEvent(QMouseEvent *event) {
    if ((!isSpotRemovalMode && !isTeethWhiteningMode) || originalImage.empty()) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // photoScreen 위젯 내에서의 상대적 위치 계산
        QPoint globalPos = event->globalPosition().toPoint();
        QPoint labelGlobalPos = ui->photoScreen->mapToGlobal(QPoint(0, 0));
        QPoint labelPos = globalPos - labelGlobalPos;

        // photoScreen 위젯의 크기
        QSize labelSize = ui->photoScreen->size();

        // 라벨 내부인지 확인
        if (labelPos.x() >= 0 && labelPos.y() >= 0 &&
            labelPos.x() < labelSize.width() && labelPos.y() < labelSize.height()) {

            QPixmap pixmap = ui->photoScreen->pixmap(Qt::ReturnByValue);
            if (!pixmap.isNull()) {
                // 이미지가 라벨에 ScaledContents로 표시되므로 직접 비율 계산
                float scaleX = (float)originalImage.cols / labelSize.width();
                float scaleY = (float)originalImage.rows / labelSize.height();

                int imageX = (int)(labelPos.x() * scaleX);
                int imageY = (int)(labelPos.y() * scaleY);

                // 이미지 경계 확인
                if (imageX >= 0 && imageY >= 0 && imageX < originalImage.cols && imageY < originalImage.rows) {
                    drawing = true;
                    lastPoint = cv::Point(imageX, imageY);

                    if (isSpotRemovalMode) {
                        // 잡티 제거
                        applyInpaintSpot(spotSmoothImage, lastPoint, 2);    // 매우 작은 inpaint
                        applySmoothSpot(spotSmoothImage, lastPoint, 3);     // 매우 작은 smooth
                        qDebug() << "Enhanced spot removal applied at:" << imageX << "," << imageY;
                    } else if (isTeethWhiteningMode) {
                        // 치아 미백 (더 작은 크기로)
                        applyTeethWhitening(spotSmoothImage, lastPoint, 6);  // 치아 미백 적용 크기 줄임
                        qDebug() << "Manual teeth whitening applied at:" << imageX << "," << imageY;
                    }

                    applyAllEffects();
                    qDebug() << "Action applied at:" << imageX << "," << imageY
                             << "Label pos:" << labelPos.x() << "," << labelPos.y()
                             << "Scale:" << scaleX << "," << scaleY;
                }
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void PhotoEditPage::mouseMoveEvent(QMouseEvent *event) {
    if ((!isSpotRemovalMode && !isTeethWhiteningMode) || !drawing || originalImage.empty()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // photoScreen 위젯 내에서의 상대적 위치 계산
    QPoint globalPos = event->globalPosition().toPoint();
    QPoint labelGlobalPos = ui->photoScreen->mapToGlobal(QPoint(0, 0));
    QPoint labelPos = globalPos - labelGlobalPos;

    // photoScreen 위젯의 크기
    QSize labelSize = ui->photoScreen->size();

    // 라벨 내부인지 확인
    if (labelPos.x() >= 0 && labelPos.y() >= 0 &&
        labelPos.x() < labelSize.width() && labelPos.y() < labelSize.height()) {

        QPixmap pixmap = ui->photoScreen->pixmap(Qt::ReturnByValue);
        if (!pixmap.isNull()) {
            // 이미지가 라벨에 ScaledContents로 표시되므로 직접 비율 계산
            float scaleX = (float)originalImage.cols / labelSize.width();
            float scaleY = (float)originalImage.rows / labelSize.height();

            int imageX = (int)(labelPos.x() * scaleX);
            int imageY = (int)(labelPos.y() * scaleY);

            if (imageX >= 0 && imageY >= 0 && imageX < originalImage.cols && imageY < originalImage.rows) {
                cv::Point current(imageX, imageY);
                int dx = abs(current.x - lastPoint.x);
                int dy = abs(current.y - lastPoint.y);
                int steps = std::max(dx, dy);
                const int MAX_STEPS = 100;
                if (steps > MAX_STEPS) steps = MAX_STEPS;

                for (int i = 0; i <= steps; ++i) {
                    float t = (steps == 0) ? 0.0f : (float)i / steps;
                    int x_interp = static_cast<int>(lastPoint.x + t * (current.x - lastPoint.x));
                    int y_interp = static_cast<int>(lastPoint.y + t * (current.y - lastPoint.y));

                    if (isSpotRemovalMode) {
                        // 드래그 중에는 빠른 처리를 위해 매우 작은 smooth만 적용
                        applySmoothSpot(spotSmoothImage, cv::Point(x_interp, y_interp), 3);
                    } else if (isTeethWhiteningMode) {
                        // 치아 미백 드래그 (더 작은 크기로)
                        applyTeethWhitening(spotSmoothImage, cv::Point(x_interp, y_interp), 4);
                    }
                }

                lastPoint = current;
                applyAllEffects();
            }
        }
    }
    QWidget::mouseMoveEvent(event);
}

void PhotoEditPage::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        drawing = false;
    }
    QWidget::mouseReleaseEvent(event);
}


void PhotoEditPage::on_teeth_whiten_4_button_clicked(bool checked)
{
    isTeethWhiteningMode = checked;
    if (checked) {
        // 잡티 제거 모드 비활성화
        isSpotRemovalMode = false;
        ui->spot_remove_pen->setChecked(false);
        ui->photoScreen->setCursor(Qt::PointingHandCursor);
        qDebug() << "Manual teeth whitening mode enabled - click on teeth to whiten";
    } else {
        ui->photoScreen->setCursor(Qt::ArrowCursor);
        qDebug() << "Manual teeth whitening mode disabled";
    }
}

// ============================================================================
// 배경색 관련 함수들
// ============================================================================

void PhotoEditPage::createBackgroundWithColor(const cv::Scalar& color)
{
    // 300x400 크기의 배경 이미지 생성 (OpenCV는 BGR 순서)
    backgroundImage = cv::Mat(400, 300, CV_8UC3, color);
    qDebug() << "Created background with color: B=" << color[0] << " G=" << color[1] << " R=" << color[2];
}

cv::Mat PhotoEditPage::overlayPhotoOnBackground(const cv::Mat& photo, const cv::Mat& background)
{
    if (photo.empty() || background.empty()) {
        qDebug() << "Empty photo or background in overlay function";
        return photo.clone();
    }

    qDebug() << "Overlay function - Background size:" << background.cols << "x" << background.rows;
    qDebug() << "Overlay function - Photo size:" << photo.cols << "x" << photo.rows;

    // 300x400 배경 이미지 복사
    cv::Mat result = background.clone();

    // 사진을 배경보다 작게 만들기 (80% 크기로)
    cv::Mat resizedPhoto;
    cv::Size targetSize(240, 320); // 300x400의 80%

    // 종횡비를 유지하면서 목표 크기에 맞게 리사이즈
    float scaleX = (float)targetSize.width / photo.cols;
    float scaleY = (float)targetSize.height / photo.rows;
    float scale = std::min(scaleX, scaleY);

    int newWidth = (int)(photo.cols * scale);
    int newHeight = (int)(photo.rows * scale);

    qDebug() << "Resizing photo from" << photo.cols << "x" << photo.rows << "to" << newWidth << "x" << newHeight;

    cv::resize(photo, resizedPhoto, cv::Size(newWidth, newHeight));

    // 중앙에 배치
    int x = (300 - newWidth) / 2;
    int y = (400 - newHeight) / 2;

    qDebug() << "Placing photo at position:" << x << "," << y << "with margin around it";

    cv::Rect roi(x, y, newWidth, newHeight);
    resizedPhoto.copyTo(result(roi));

    qDebug() << "Overlay completed, result size:" << result.cols << "x" << result.rows << "with background visible";

    return result;
}

// 배경색 콤보박스 이벤트 핸들러
void PhotoEditPage::on_comboBox_background_currentTextChanged(const QString &text)
{
    // 미리 정의된 배경색 설정 (BGR 순서)
    if (text == "흰색") {
        currentBackgroundColor = cv::Scalar(255, 255, 255); // 흰색
    } else if (text == "파란색") {
        currentBackgroundColor = cv::Scalar(255, 0, 0); // 파란색
    } else if (text == "빨간색") {
        currentBackgroundColor = cv::Scalar(0, 0, 255); // 빨간색
    } else if (text == "회색") {
        currentBackgroundColor = cv::Scalar(128, 128, 128); // 회색
    }

    // 배경 이미지 새로 생성
    createBackgroundWithColor(currentBackgroundColor);

    // 현재 이미지가 있으면 applyAllEffects, 없으면 배경만 표시
    if (!originalImage.empty()) {
        applyAllEffects();
    } else {
        cv::Mat emptyMat;
        displayCurrentImage(emptyMat);
    }

    qDebug() << "Background color changed to:" << text;
}



void PhotoEditPage::on_retakeshot_button_clicked()
{
    // main_app으로 돌아가서 재촬영
    if (mainApp) {
        this->hide();
        mainApp->show();
    }
}

void PhotoEditPage::on_init_button_clicked()
{
    qDebug() << "=== 편집 초기화 버튼 클릭됨 ===";

    // 모든 효과를 초기 상태로 되돌리기

    // UI 컨트롤들을 초기값으로 설정
    qDebug() << "UI 컨트롤 초기화 중...";
    ui->BW_Button->setChecked(false);
    ui->Sharpen_bar->setValue(0);
    ui->eye_size_bar->setValue(0);
    ui->spot_remove_pen->setChecked(false);
    ui->teeth_whiten_4_button->setChecked(false);

    // 효과 상태 변수들을 초기값으로 설정
    qDebug() << "상태 변수 초기화 중...";
    isBWMode = false;
    isHorizontalFlipped = false;
    sharpnessStrength = 0;
    eyeSizeStrength = 0;
    isSpotRemovalMode = false;
    isTeethWhiteningMode = false;

    // 배경색을 기본 흰색으로 설정
    qDebug() << "배경색을 흰색으로 초기화 중...";
    currentBackgroundColor = cv::Scalar(255, 255, 255); // 화이트 (BGR)
    createBackgroundWithColor(currentBackgroundColor);

    // 원본 이미지가 있으면 currentImage와 spotSmoothImage를 원본으로 복원하고 표시
    if (!originalImage.empty()) {
        qDebug() << "원본 이미지로 복원 중...";
        currentImage = originalImage.clone();
        spotSmoothImage = originalImage.clone(); // 스팟 제거용 이미지도 원본으로 초기화
        qDebug() << "모든 이미지를 원본으로 복원 완료";
        applyAllEffects(); // 초기화된 상태로 효과 적용 (실제로는 효과 없음)
    } else {
        qDebug() << "원본 이미지가 없음, 배경만 표시";
        // 원본 이미지가 없으면 배경만 표시
        cv::Mat emptyMat;
        displayCurrentImage(emptyMat);
    }

    qDebug() << "=== 편집 초기화 완료 ===";
}

