#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream> // Added for file operations
#include <vector>  // Added for buffer

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

// 이미지 선명도 보정
void sharpen(Mat& image, int strength) {
    if (strength <= 0 || image.empty()) return;

    Size original_size = image.size();
    if (original_size.width <= 0 || original_size.height <= 0 ||
        original_size.width > 10000 || original_size.height > 10000) return;

    // Check for potential overflow before multiplication
    if (original_size.width > INT_MAX / 2 || original_size.height > INT_MAX / 2) {
        std::cerr << "Warning: Image dimensions too large for sharpening, potential overflow. Skipping." << std::endl;
        return;
    }

    int new_width = original_size.width * 2;
    int new_height = original_size.height * 2;
    if (new_width > 3000 || new_height > 3000) return;

    Mat upscaled;
    resize(image, upscaled, Size(new_width, new_height), 0, 0, INTER_CUBIC);
    Mat blurred;
    GaussianBlur(upscaled, blurred, Size(0, 0), 3);
    float amount = strength / 10.0f;
    addWeighted(upscaled, 1.0f + amount, blurred, -amount, 0, upscaled);
    resize(upscaled, image, original_size, 0, 0, INTER_AREA);
}

// 눈 자체 확대
void correctEyes(Mat& image, Rect roi, float squeeze_factor) {
    if (squeeze_factor <= 0 || image.empty()) return;

    Rect upper_face_roi(roi.x, roi.y, roi.width, roi.height / 2);
    if (upper_face_roi.width <= 0 || upper_face_roi.height <= 0) return;

    vector<Rect> eyes;
    Mat gray;
    cvtColor(image(upper_face_roi), gray, COLOR_BGR2GRAY);
    eye_cascade.detectMultiScale(gray, eyes, 1.1, 3, 0, Size(25, 25));
    if (eyes.size() != 2) return;

    for (const auto& eye_roi : eyes) {
        Mat eye = image(upper_face_roi)(eye_roi).clone();
        if (eye.empty()) continue;

        float scale = min(1.0f + squeeze_factor * 0.5f, 1.5f);
        int new_width = min(static_cast<int>(eye.cols * scale), 300);
        int new_height = min(static_cast<int>(eye.rows * scale), 300);
        if (new_width <= 0 || new_height <= 0) continue;

        Mat enlarged_eye;
        resize(eye, enlarged_eye, Size(new_width, new_height), 0, 0, INTER_CUBIC);

        Point center(eye_roi.x + eye_roi.width / 2, eye_roi.y + eye_roi.height / 2);
        Rect target(center.x - new_width / 2, center.y - new_height / 2, new_width, new_height);
        target &= Rect(0, 0, upper_face_roi.width, upper_face_roi.height);
        if (target.width <= 0 || target.height <= 0) continue;

        Mat mask = Mat::zeros(enlarged_eye.size(), CV_8UC1);
        circle(mask, Point(mask.cols / 2, mask.rows / 2), min(mask.cols, mask.rows) / 2, Scalar(255), -1);
        GaussianBlur(mask, mask, Size(15, 15), 5);

        Mat roi_area = image(upper_face_roi)(target);
        Mat blended;
        roi_area.copyTo(blended);
        enlarged_eye.copyTo(blended, mask);
        blended.copyTo(image(upper_face_roi)(target));
    }
}

// 잡티 복구
void applySmoothSpot(Mat& image, const Point& center, int radius) {
    if (image.empty()) return;
    Rect roi(center.x - radius, center.y - radius, 2 * radius, 2 * radius);
    roi &= Rect(0, 0, image.cols, image.rows);
    if (roi.area() == 0) return;

    Mat roi_image = image(roi);
    Mat smoothed;
    bilateralFilter(roi_image, smoothed, 9, 75, 75);

    Mat mask = Mat::zeros(roi.size(), CV_8UC1);
    circle(mask, Point(radius, radius), radius, Scalar(255), -1);
    smoothed.copyTo(roi_image, mask);
}

// 트랙바 콜백
void on_trackbar(int, void*) {
    if (spot_smoothed_image.empty() || original_image.empty()) return;

    Mat current = spot_smoothed_image.clone();
    if (face_roi.width <= 0 || face_roi.height <= 0) return;

    Mat face_area = current(face_roi);
    if (face_area.empty()) return;

    if (sharpen_strength > 0) sharpen(face_area, sharpen_strength);

    float factor = eye_squeeze / 100.0f;
    if (factor > 0) correctEyes(current, face_roi, factor);

    if (bw_on > 0) {
        cvtColor(current, current, COLOR_BGR2GRAY);
        cvtColor(current, current, COLOR_GRAY2BGR);
    }

    if (original_image.empty() || current.empty()) return;

    Mat combined;
    hconcat(original_image, current, combined);
    if (combined.empty()) return;

    const int MAX_WIDTH = 1600;
    if (combined.cols > MAX_WIDTH) {
        display_ratio = (double)MAX_WIDTH / combined.cols;
        resize(combined, combined, Size(), display_ratio, display_ratio, INTER_AREA);
    } else {
        display_ratio = 1.0;
    }

    imshow("증명사진 보정", combined);
}

// 마우스 콜백
void on_mouse(int event, int x, int y, int, void*) {
    if (original_image.empty()) return;

    int cx = (int)(x / display_ratio);
    int cy = (int)(y / display_ratio);
    cx = max(0, min(cx, original_image.cols - 1));
    cy = max(0, min(cy, original_image.rows - 1));

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

        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / steps;
            int x_interp = last_point.x + t * (current.x - last_point.x);
            int y_interp = last_point.y + t * (current.y - last_point.y);
            applySmoothSpot(spot_smoothed_image, Point(x_interp, y_interp), 10);
        }

        last_point = current;
        on_trackbar(0, 0);
    } else if (event == EVENT_LBUTTONUP) {
        drawing = false;
    }
}

int main() {
    // 이미지 파일 열기
    ifstream file("face.jpeg", ios::binary | ios::ate); // ios::ate로 파일 끝으로 이동
    if (!file.is_open()) {
        cout << "이미지(face.jpeg)를 불러올 수 없습니다." << endl;
        return -1;
    }

    // 파일 크기 확인
    streampos file_size = file.tellg();
    file.seekg(0, ios::beg); // 파일 시작으로 다시 이동

    const streampos MAX_FILE_SIZE_BYTES = 500 * 1024 * 1024; // 500MB 제한 (조정 가능)
    if (file_size > MAX_FILE_SIZE_BYTES) {
        cout << "오류: face.jpeg 파일이 너무 큽니다 (" << file_size / (1024 * 1024) << "MB). 최대 "
             << MAX_FILE_SIZE_BYTES / (1024 * 1024) << "MB 이하의 이미지를 사용해주세요." << endl;
        return -1;
    }

    // 파일을 버퍼로 읽어 들입니다.
    size_t actual_file_size = static_cast<size_t>(file_size); // 명시적으로 size_t로 캐스팅
    if (actual_file_size == 0) {
        cout << "오류: face.jpeg 파일이 비어 있습니다." << endl;
        return -1;
    }
    vector<uchar> buffer(actual_file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), actual_file_size);
    file.close();

    // 먼저 크기를 줄여서 디코딩을 시도합니다.
    original_image = imdecode(buffer, IMREAD_COLOR | IMREAD_REDUCED_COLOR_2); // 1/2 크기로 디코딩
    if (original_image.empty()) {
        // 크기를 줄인 디코딩이 실패하면, 전체 크기로 디코딩을 시도합니다 (여전히 메모리 부족 오류가 발생할 수 있음).
        // 하지만 파일 크기 제한으로 인해 이 단계에서 OOM이 발생할 가능성은 줄어듭니다.
        original_image = imdecode(buffer, IMREAD_COLOR);
        if (original_image.empty()) {
            cout << "이미지(face.jpeg)를 디코딩할 수 없습니다." << endl;
            return -1;
        }
    }

    // 초기 크기 축소 디코딩 후에도 여전히 너무 크다면 추가로 크기를 조정합니다.
    const int MAX_DIMENSION = 1920; // 처리할 최대 이미지 크기
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
    cv::Rect detected_face_rect;
    if (faces.empty()) {
        detected_face_rect = Rect(0, 0, original_image.cols, original_image.rows);
    } else {
        detected_face_rect = faces[0];
    }

    // Ensure the detected face rectangle is within the bounds of the original_image
    int x = std::max(0, detected_face_rect.x);
    int y = std::max(0, detected_face_rect.y);
    int w = std::min(detected_face_rect.width, original_image.cols - x);
    int h = std::min(detected_face_rect.height, original_image.rows - y);

    // If the calculated width or height is non-positive, it means the face is invalid or outside the frame.
    // In this case, default to the entire image or handle as an error.
    if (w <= 0 || h <= 0) {
        std::cerr << "Warning: Detected face rectangle is invalid or outside image boundaries. Using full image as ROI." << std::endl;
        face_roi = Rect(0, 0, original_image.cols, original_image.rows);
    } else {
        face_roi = Rect(x, y, w, h);
    }

    spot_smoothed_image = original_image.clone();

    namedWindow("증명사진 보정");
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
