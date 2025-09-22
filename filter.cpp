#include <opencv2/opencv.hpp>
#include <iostream>
#include <algorithm>

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
int eye_squeeze = 0;

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
    if (x < -1000000 || y < -1000000 || x > 1000000 || y > 1000000) return Rect();
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
    int new_width = original_size.width * 2;
    int new_height = original_size.height * 2;

    const int UPSCALE_LIMIT = 1500;
    if (new_width > UPSCALE_LIMIT || new_height > UPSCALE_LIMIT) return;

    Mat upscaled;
    resize(image, upscaled, Size(new_width, new_height), 0, 0, INTER_CUBIC);

    Mat blurred;
    GaussianBlur(upscaled, blurred, Size(0, 0), 3);

    float amount = strength / 10.0f;
    addWeighted(upscaled, 1.0f + amount, blurred, -amount, 0, upscaled);

    resize(upscaled, image, original_size, 0, 0, INTER_AREA);
}

// 눈 확대
void correctEyes(Mat& image, Rect roi, float squeeze_factor) {
    if (squeeze_factor <= 0 || image.empty()) return;

    Rect safe_face = safeRect(roi.x, roi.y, roi.width, roi.height, image.cols, image.rows);
    if (safe_face.empty()) return;

    Rect upper_face_roi(safe_face.x, safe_face.y, safe_face.width, max(1, safe_face.height / 2));
    if (upper_face_roi.width <= 0 || upper_face_roi.height <= 0) return;

    Mat roi_img = image(upper_face_roi);
    if (roi_img.empty()) return;

    vector<Rect> eyes;
    Mat gray;
    cvtColor(roi_img, gray, COLOR_BGR2GRAY);
    eye_cascade.detectMultiScale(gray, eyes, 1.1, 3, 0, Size(25, 25));
    if (eyes.size() != 2) return;

    for (const auto& eye_roi_raw : eyes) {
        Rect eye_roi = safeRect(eye_roi_raw.x, eye_roi_raw.y, eye_roi_raw.width, eye_roi_raw.height,
                                upper_face_roi.width, upper_face_roi.height);
        if (eye_roi.empty()) continue;

        Mat eye = roi_img(eye_roi).clone();
        if (eye.empty()) continue;

        float scale = min(1.0f + squeeze_factor * 0.5f, 1.5f);
        int new_width = min(static_cast<int>(eye.cols * scale), 300);
        int new_height = min(static_cast<int>(eye.rows * scale), 300);

        Mat enlarged_eye;
        resize(eye, enlarged_eye, Size(new_width, new_height), 0, 0, INTER_CUBIC);

        Point center(eye_roi.x + eye_roi.width / 2, eye_roi.y + eye_roi.height / 2);
        int tx = center.x - new_width / 2;
        int ty = center.y - new_height / 2;
        Rect target = safeRect(tx, ty, new_width, new_height, upper_face_roi.width, upper_face_roi.height);
        if (target.empty()) continue;

        Mat mask = Mat::zeros(enlarged_eye.size(), CV_8UC1);
        circle(mask, Point(mask.cols / 2, mask.rows / 2), min(mask.cols, mask.rows) / 2, Scalar(255), -1);
        GaussianBlur(mask, mask, Size(15, 15), 5);

        Mat roi_area = roi_img(target);
        if (roi_area.empty()) continue;

        Mat resized_enlarged, resized_mask;
        if (enlarged_eye.size() != roi_area.size()) {
            resize(enlarged_eye, resized_enlarged, roi_area.size(), 0, 0, INTER_CUBIC);
            resize(mask, resized_mask, roi_area.size(), 0, 0, INTER_LINEAR);
        } else {
            resized_enlarged = enlarged_eye;
            resized_mask = mask;
        }

        Mat blended;
        roi_area.copyTo(blended);
        resized_enlarged.copyTo(blended, resized_mask);
        blended.copyTo(roi_img(target));
    }

    roi_img.copyTo(image(upper_face_roi));
}

// 잡티 제거
void applySmoothSpot(Mat& image, const Point& center, int radius) {
    if (image.empty()) return;
    if (!isValidSize(image, 5000)) return;
    if (radius <= 0 || radius > 200) return;

    Rect roi = safeRect(center.x - radius, center.y - radius, 2 * radius, 2 * radius, image.cols, image.rows);
    if (roi.empty()) return;

    // ROI 크기 제한 (안전)
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
    if (face_roi.width <= 0 || face_roi.height <= 0) return;

    Rect safe_face = safeRect(face_roi.x, face_roi.y, face_roi.width, face_roi.height, current.cols, current.rows);
    if (safe_face.empty()) return;

    Mat face_area = current(safe_face);
    if (sharpen_strength > 0) sharpen(face_area, sharpen_strength);

    float factor = eye_squeeze / 100.0f;
    if (factor > 0) correctEyes(current, safe_face, factor);

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
    if (display_ratio <= 0.0) display_ratio = 1.0; // 방어

    int cx = static_cast<int>(x / display_ratio);
    int cy = static_cast<int>(y / display_ratio);

    if (cx < 0 || cy < 0 || cx >= original_image.cols || cy >= original_image.rows) {
        cerr << "[on_mouse] invalid click (" << cx << "," << cy << ")" << endl;
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
    original_image = imread("face.jpeg");
    if (original_image.empty()) {
        cout << "이미지(face.jpeg)를 불러올 수 없습니다." << endl;
        return -1;
    }

    const int MAX_DIMENSION = 1000;
    if (original_image.cols > MAX_DIMENSION || original_image.rows > MAX_DIMENSION) {
        double scale = (double)MAX_DIMENSION / max(original_image.cols, original_image.rows);
        resize(original_image, original_image, Size(), scale, scale, INTER_AREA);
    }

    if (!face_cascade.load("haarcascade_frontalface_default.xml") ||
        !eye_cascade.load("haarcascade_eye.xml")) {
        cout << "분류기를 로드할 수 없습니다." << endl;
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
    createTrackbar("눈 크기", "증명사진 보정", &eye_squeeze, 100, on_trackbar);

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
            eye_squeeze = 0;
            on_trackbar(0, 0);
        }
    }

    destroyAllWindows();
    return 0;
}
