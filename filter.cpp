#include <opencv2/opencv.hpp>
#include <iostream>
#include <cmath>

using namespace cv;
using namespace std;

// 전역 변수
Mat original_image;
Rect face_roi;
Mat inpaint_mask;
Point last_point;
bool drawing = false;
double display_ratio = 1.0;

// 트랙바 변수
int sharpen_strength = 0;
int bw_on = 0;
int blemish_strength = 0;
int chin_squeeze = 0;
int redness_reduction = 0;

// 이미지 처리 함수
void sharpen(Mat& image, int strength) {
    if (strength <= 0) return;
    Size original_size = image.size();
    Mat upscaled;
    resize(image, upscaled, Size(), 2, 2, INTER_CUBIC);
    Mat blurred;
    GaussianBlur(upscaled, blurred, Size(0, 0), 3);
    float amount = (float)strength / 10.0f;
    addWeighted(upscaled, 1.0f + amount, blurred, -amount, 0, upscaled);
    resize(upscaled, image, original_size, 0, 0, INTER_AREA);
}

void removeBlemishes(Mat& image, int d, int sigma) {
    if (d <= 1 || sigma <= 1) return;
    Mat result;
    bilateralFilter(image, result, d, sigma, sigma);
    image = result;
}

void correctChinLine(Mat& image, Rect roi, float squeeze_factor) {
    if (squeeze_factor <= 0) return;
    int chin_height = roi.height / 3;
    int start_y = roi.y + roi.height - chin_height;
    Rect chin_area_rect(roi.x, start_y, roi.width, chin_height);
    chin_area_rect &= Rect(0, 0, image.cols, image.rows);
    if (chin_area_rect.width == 0 || chin_area_rect.height == 0) return;

    Mat chin_area = image(chin_area_rect).clone();
    Mat map_x(chin_area.size(), CV_32FC1);
    Mat map_y(chin_area.size(), CV_32FC1);

    for (int y = 0; y < chin_area.rows; y++) {
        for (int x = 0; x < chin_area.cols; x++) {
            float progress = (float)y / chin_area.rows;
            float dx = x - chin_area.cols / 2.0f;
            float scale = 1.0f - progress * squeeze_factor;
            float new_dx = dx * scale;
            map_x.at<float>(y, x) = chin_area.cols / 2.0f + new_dx;
            map_y.at<float>(y, x) = y;
        }
    }
    Mat warped_chin_area;
    remap(chin_area, warped_chin_area, map_x, map_y, INTER_LINEAR, BORDER_REPLICATE);
    warped_chin_area.copyTo(image(chin_area_rect));
}

// 🔥 붉은기 강력 제거 함수
void reduceRednessStrong(Mat& image, int strength) {
    if (strength <= 0) return;
    Mat hsv;
    cvtColor(image, hsv, COLOR_BGR2HSV);

    Mat mask1, mask2;
    inRange(hsv, Scalar(0, 30, 80), Scalar(25, 255, 255), mask1);
    inRange(hsv, Scalar(160, 30, 80), Scalar(180, 255, 255), mask2);
    Mat red_mask = mask1 | mask2;

    GaussianBlur(red_mask, red_mask, Size(21, 21), 0);

    float r_scale = 1.0f - strength / 40.0f;
    float g_boost = 1.0f + strength / 200.0f;
    float b_boost = 1.0f + strength / 300.0f;

    Mat result = image.clone();
    for (int y = 0; y < image.rows; y++) {
        for (int x = 0; x < image.cols; x++) {
            if (red_mask.at<uchar>(y, x) > 50) {
                Vec3b& pixel = result.at<Vec3b>(y, x);
                pixel[2] = saturate_cast<uchar>(pixel[2] * r_scale);
                pixel[1] = saturate_cast<uchar>(pixel[1] * g_boost);
                pixel[0] = saturate_cast<uchar>(pixel[0] * b_boost);
            }
        }
    }

    image = result;
}

// 트랙바 콜백
void on_trackbar(int, void*) {
    Mat processed_image = original_image.clone();

    if (!inpaint_mask.empty() && countNonZero(inpaint_mask) > 0) {
        inpaint(original_image, inpaint_mask, processed_image, 10, INPAINT_TELEA);
    }

    Mat face_area = processed_image(face_roi);

    if (blemish_strength > 0) {
        int d = blemish_strength / 10 + 5;
        if (d % 2 == 0) d++;
        int sigma = blemish_strength * 5;
        removeBlemishes(face_area, d, sigma);
    }

    if (sharpen_strength > 0) {
        sharpen(face_area, sharpen_strength);
    }

    float squeeze_factor = (float)chin_squeeze / 100.0f;
    if (squeeze_factor > 0) {
        correctChinLine(processed_image, face_roi, squeeze_factor);
    }

    if (redness_reduction > 0) {
        reduceRednessStrong(face_area, redness_reduction);
    }

    if (bw_on > 0) {
        cvtColor(processed_image, processed_image, COLOR_BGR2GRAY);
        cvtColor(processed_image, processed_image, COLOR_GRAY2BGR);
    }

    Mat combined_image;
    hconcat(original_image, processed_image, combined_image);

    const int MAX_WIDTH = 1600;
    if (combined_image.cols > MAX_WIDTH) {
        display_ratio = (double)MAX_WIDTH / combined_image.cols;
        resize(combined_image, combined_image, Size(), display_ratio, display_ratio, INTER_AREA);
    } else {
        display_ratio = 1.0;
    }

    imshow("증명사진 보정", combined_image);
}

// 마우스 콜백
void on_mouse(int event, int x, int y, int flags, void* userdata) {
    int corrected_x = (int)(x / display_ratio);
    int corrected_y = (int)(y / display_ratio);

    if (corrected_x < 0 || corrected_y < 0 || corrected_x >= original_image.cols || corrected_y >= original_image.rows) {
        return;
    }

    if (event == EVENT_LBUTTONDOWN) {
        drawing = true;
        last_point = Point(corrected_x, corrected_y);
        circle(inpaint_mask, last_point, 5, Scalar(255), -1);
        on_trackbar(0, 0);
    } else if (event == EVENT_MOUSEMOVE && drawing) {
        line(inpaint_mask, last_point, Point(corrected_x, corrected_y), Scalar(255), 10);
        last_point = Point(corrected_x, corrected_y);
        on_trackbar(0, 0);
    } else if (event == EVENT_LBUTTONUP) {
        drawing = false;
    }
}

int main() {
    original_image = imread("face.jpeg");
    if (original_image.empty()) {
        cout << "이미지(face.jpeg)를 로드할 수 없습니다." << endl;
        return -1;
    }

    CascadeClassifier face_cascade;
    if (!face_cascade.load("haarcascade_frontalface_default.xml")) {
        cout << "얼굴 감지기를 로드할 수 없습니다." << endl;
        return -1;
    }

    vector<Rect> faces;
    Mat face_gray;
    cvtColor(original_image, face_gray, COLOR_BGR2GRAY);
    equalizeHist(face_gray, face_gray);
    face_cascade.detectMultiScale(face_gray, faces, 1.1, 5, 0, Size(80, 80));
    if (faces.empty()) {
        cout << "얼굴을 감지할 수 없습니다." << endl;
        face_roi = Rect(0, 0, original_image.cols, original_image.rows);
    } else {
        face_roi = faces[0];
    }

    namedWindow("증명사진 보정");
    setMouseCallback("증명사진 보정", on_mouse, NULL);

    createTrackbar("이미지 선명도", "증명사진 보정", &sharpen_strength, 10, on_trackbar);
