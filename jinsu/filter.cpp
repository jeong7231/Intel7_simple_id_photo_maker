#include <opencv2/opencv.hpp>
#include <iostream>
#include <algorithm>
#include <string>

using namespace cv;
using namespace std;

// 전역 변수
Mat original_image, spot_smoothed_image;
Rect face_roi;
Point last_point;
bool drawing = false;
double display_ratio = 1.0;

// 트랙바 변수
int sharpen_strength = 0;
int bw_on = 0;
int eye_enlargement_strength = 0;

// 캐스케이드 분류기
CascadeClassifier face_cascade, eye_cascade;

// 유틸: Mat 크기 유효성 검사
bool isValidSize(const Mat& m, int maxSize = 5000) {
    if (m.empty()) return false;
    if (m.cols <= 0 || m.rows <= 0) return false;
    if (m.cols > maxSize || m.rows > maxSize) return false;
    return true;
}

// 유틸: 안전한 Rect 생성
Rect safeRect(int x, int y, int w, int h, int maxW, int maxH) {
    if (w <= 0 || h <= 0) return Rect();
    Rect r(x, y, w, h);
    Rect bounds(0, 0, maxW, maxH);
    r &= bounds;
    if (r.width <= 0 || r.height <= 0) return Rect();
    return r;
}

// 이미지 선명도 보정
void sharpen(Mat& image, int strength) {
    if (strength <= 0 || image.empty()) return;
    if (!isValidSize(image, 3000)) return;

    Size original_size = image.size();
    const int UPSCALE_LIMIT = 1500;

    Mat upscaled;
    double upscale_factor = 2.0;
    Size new_size(original_size.width * upscale_factor, original_size.height * upscale_factor);

    if (new_size.width > UPSCALE_LIMIT || new_size.height > UPSCALE_LIMIT) {
        upscale_factor = (double)UPSCALE_LIMIT / max(original_size.width, original_size.height);
        new_size = Size(original_size.width * upscale_factor, original_size.height * upscale_factor);
    }
    
    if (new_size.width <= 0 || new_size.height <= 0) return;

    resize(image, upscaled, new_size, 0, 0, INTER_CUBIC);

    Mat blurred;
    GaussianBlur(upscaled, blurred, Size(0, 0), 3);

    float amount = strength / 10.0f;
    addWeighted(upscaled, 1.0f + amount, blurred, -amount, 0, upscaled);

    resize(upscaled, image, original_size, 0, 0, INTER_AREA);
}

// 눈 영역을 확대하여 블렌딩하는 기능
void correctEyes(Mat& image, Rect roi, int strength) {
    if (strength <= 0 || image.empty()) return;

    // 포토샵 효과처럼 더 강력한 확대를 위한 배율
    float enlargement_factor = strength / 70.0f;
    if (enlargement_factor <= 0) return;

    Rect safe_face = safeRect(roi.x, roi.y, roi.width, roi.height, image.cols, image.rows);
    if (safe_face.empty()) return;

    Rect upper_face_roi(safe_face.x, safe_face.y, safe_face.width, max(1, safe_face.height * 2 / 3));
    if (upper_face_roi.width <= 0 || upper_face_roi.height <= 0) return;

    Mat roi_img = image(upper_face_roi);
    if (roi_img.empty()) return;

    vector<Rect> eyes;
    Mat gray;
    cvtColor(roi_img, gray, COLOR_BGR2GRAY);
    // 히스토그램 평활화로 대비를 개선하여 눈 감지율을 높임
    equalizeHist(gray, gray);
    // 안경 쓴 눈 감지 모델을 고려하여 파라미터 조정
    eye_cascade.detectMultiScale(gray, eyes, 1.1, 4, 0, Size(20, 20));

    if (eyes.size() != 2) return;
    
    sort(eyes.begin(), eyes.end(), [](const Rect& a, const Rect& b) {
        return a.x < b.x;
    });

    Mat original_roi_img = roi_img.clone();

    for (const auto& eye_roi_raw : eyes) {
        Rect eye_roi = safeRect(eye_roi_raw.x, eye_roi_raw.y, eye_roi_raw.width, eye_roi_raw.height,
                                roi_img.cols, roi_img.rows);
        if (eye_roi.empty()) continue;

        // 1. ROI를 눈 중심의 정사각형으로 정제
        int side = min(eye_roi.width, eye_roi.height);
        Point center(eye_roi.x + eye_roi.width / 2, eye_roi.y + eye_roi.height / 2);
        Rect square_eye_roi(center.x - side / 2, center.y - side / 2, side, side);
        square_eye_roi &= Rect(0, 0, roi_img.cols, roi_img.rows);
        if (square_eye_roi.empty()) continue;

        // 2. 정제된 눈 영역을 잘라냄
        Mat original_eye_area = original_roi_img(square_eye_roi);
        if (original_eye_area.empty()) continue;

        // 3. 잘라낸 영역을 확대
        float scale = 1.0f + enlargement_factor;
        int new_size = static_cast<int>(side * scale);
        if (new_size <= side) continue;
        
        Mat enlarged_eye;
        resize(original_eye_area, enlarged_eye, Size(new_size, new_size), 0, 0, INTER_CUBIC);

        // 4. 확대된 이미지를 붙여넣을 위치 계산
        Rect target_roi(center.x - new_size / 2, center.y - new_size / 2, new_size, new_size);
        target_roi &= Rect(0, 0, roi_img.cols, roi_img.rows);
        if (target_roi.empty()) continue;

        // 5. 부드러운 합성을 위한 원형 마스크 생성
        Mat mask = Mat::zeros(enlarged_eye.size(), CV_8UC1);
        circle(mask, Point(mask.cols / 2, mask.rows / 2), mask.cols / 2, Scalar(255), -1);
        GaussianBlur(mask, mask, Size(21, 21), 10);

        // 확대된 이미지와 마스크를 최종 타겟 크기에 맞게 리사이즈
        resize(enlarged_eye, enlarged_eye, target_roi.size());
        resize(mask, mask, target_roi.size());

        // 6. 마스크를 이용해 원본에 합성
        enlarged_eye.copyTo(roi_img(target_roi), mask);
    }
}

// 잡티 제거
void applySmoothSpot(Mat& image, const Point& center, int radius) {
    if (image.empty()) return;
    if (!isValidSize(image, 5000)) return;
    if (radius <= 0 || radius > 200) return;

    Rect roi = safeRect(center.x - radius, center.y - radius, 2 * radius, 2 * radius, image.cols, image.rows);
    if (roi.empty()) return;

    if (roi.width > 200 || roi.height > 200) {
        cerr << "[applySmoothSpot] ROI too big: " << roi.width << "x" << roi.height << endl;
        return;
    }

    Mat roi_image = image(roi);
    if (roi_image.empty()) return;

    Mat smoothed;
    bilateralFilter(roi_image, smoothed, 9, 75, 75);

    Mat mask = Mat::zeros(roi.size(), CV_8UC1);
    circle(mask, Point(roi.width / 2, roi.height / 2), min(roi.width, roi.height) / 2, Scalar(255), -1);
    GaussianBlur(mask, mask, Size(15, 15), 5);

    smoothed.copyTo(roi_image, mask);
}

// 트랙바 콜백
void on_trackbar(int, void*) {
    if (spot_smoothed_image.empty() || original_image.empty()) return;

    Mat current = spot_smoothed_image.clone();

    // 얼굴 ROI를 안전하게 정의
    Rect safe_face = safeRect(face_roi.x, face_roi.y, face_roi.width, face_roi.height, current.cols, current.rows);
    if (!safe_face.empty()) {
        Mat face_area = current(safe_face);
        // 선명도 보정 적용
        if (sharpen_strength > 0) sharpen(face_area, sharpen_strength);
    }
    
    // 눈 크기 조절 기능 적용
    if (eye_enlargement_strength > 0) {
        correctEyes(current, face_roi, eye_enlargement_strength);
    }

    // 흑백 변환 적용
    if (bw_on > 0) {
        cvtColor(current, current, COLOR_BGR2GRAY);
        cvtColor(current, current, COLOR_GRAY2BGR);
    }

    Mat combined;
    try {
        hconcat(original_image, current, combined);
    } catch (const cv::Exception& e) {
        cerr << "[on_trackbar] hconcat failed: " << e.what() << endl;
        return;
    }

    const int MAX_WIDTH = 1600;
    if (combined.cols > MAX_WIDTH) {
        display_ratio = (double)MAX_WIDTH / combined.cols;
        if (display_ratio <= 0 || display_ratio > 1.0) display_ratio = 1.0;
        resize(combined, combined, Size(), display_ratio, display_ratio, INTER_AREA);
    } else {
        display_ratio = 1.0;
    }

    imshow("증명사진 보정", combined);
}

// 마우스 콜백
void on_mouse(int event, int x, int y, int, void*) {
    if (original_image.empty()) return;
    if (display_ratio <= 0.0) display_ratio = 1.0;

    int cx = static_cast<int>(x / display_ratio);
    int cy = static_cast<int>(y / display_ratio);

    if (cx < 0 || cy < 0 || cx >= original_image.cols || cy >= original_image.rows) {
        return;
    }

    if (event == EVENT_LBUTTONDOWN) {
        drawing = true;
        last_point = Point(cx, cy);
        applySmoothSpot(spot_smoothed_image, last_point, 10);
        on_trackbar(0, 0);
    } else if (event == EVENT_MOUSEMOVE && drawing) {
        Point current(cx, cy);
        int dx = abs(current.x - last_point.x);
        int dy = abs(current.y - last_point.y);
        int steps = max(dx, dy);
        const int MAX_STEPS = 1000;
        if (steps > MAX_STEPS) steps = MAX_STEPS;

        for (int i = 0; i <= steps; ++i) {
            float t = (steps == 0) ? 0.0f : (float)i / steps;
            int x_interp = static_cast<int>(last_point.x + t * (current.x - last_point.x));
            int y_interp = static_cast<int>(last_point.y + t * (current.y - last_point.y));
            applySmoothSpot(spot_smoothed_image, Point(x_interp, y_interp), 10);
        }

        last_point = current;
        on_trackbar(0, 0);
    } else if (event == EVENT_LBUTTONUP) {
        drawing = false;
    }
}

int main() {
    original_image = imread("/home/ubuntu/opencv/Intel7_simple_id_photo_maker/face.jpeg");
    if (original_image.empty()) {
        cout << "이미지(/home/ubuntu/opencv/Intel7_simple_id_photo_maker/face.jpeg)를 불러올 수 없습니다." << endl;
        return -1;
    }

    const int MAX_DIMENSION = 1000;
    if (original_image.cols > MAX_DIMENSION || original_image.rows > MAX_DIMENSION) {
        double scale = (double)MAX_DIMENSION / max(original_image.cols, original_image.rows);
        resize(original_image, original_image, Size(), scale, scale, INTER_AREA);
    }

    if (!face_cascade.load("/home/ubuntu/opencv/Intel7_simple_id_photo_maker/haarcascade_frontalface_default.xml") ||
        !eye_cascade.load("/home/ubuntu/opencv/Intel7_simple_id_photo_maker/haarcascade_eye_tree_eyeglasses.xml")) {
        cout << "분류기 파일을 찾을 수 없습니다. 경로를 확인해주세요." << endl;
        return -1;
    }

    Mat gray;
    cvtColor(original_image, gray, COLOR_BGR2GRAY);
    equalizeHist(gray, gray);
    vector<Rect> faces;
    face_cascade.detectMultiScale(gray, faces, 1.1, 5, 0, Size(80, 80));
    if (!faces.empty()) {
        Rect f = faces[0];
        Rect safe_f = safeRect(f.x, f.y, f.width, f.height, original_image.cols, original_image.rows);
        face_roi = safe_f.empty() ? Rect(0, 0, original_image.cols, original_image.rows) : safe_f;
    } else {
        face_roi = Rect(0, 0, original_image.cols, original_image.rows);
    }

    spot_smoothed_image = original_image.clone();

    namedWindow("증명사진 보정", WINDOW_AUTOSIZE);
    setMouseCallback("증명사진 보정", on_mouse, NULL);
    createTrackbar("이미지 선명도", "증명사진 보정", &sharpen_strength, 10, on_trackbar);
    createTrackbar("흑백", "증명사진 보정", &bw_on, 1, on_trackbar);
    createTrackbar("눈 확대", "증명사진 보정", &eye_enlargement_strength, 10, on_trackbar);

    on_trackbar(0, 0);

    cout << "\n[스팟 복구 사용법]" << endl;
    cout << "마우스를 드래그하여 잡티 영역을 부드럽게 하세요." << endl;
    cout << "'r' 키를 누르면 모든 보정을 리셋합니다. 'q' 또는 'ESC' 키로 종료합니다." << endl;

    while (true) {
        int key = waitKey(0);
        if (key == 'q' || key == 27) break;
        else if (key == 'r') {
            spot_smoothed_image = original_image.clone();
            sharpen_strength = 0;
            bw_on = 0;
            eye_enlargement_strength = 0;
            setTrackbarPos("이미지 선명도", "증명사진 보정", sharpen_strength);
            setTrackbarPos("흑백", "증명사진 보정", bw_on);
            setTrackbarPos("눈 확대", "증명사진 보정", eye_enlargement_strength);
            on_trackbar(0, 0);
        }
    }

    destroyAllWindows();
    return 0;
}
