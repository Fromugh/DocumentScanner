#include <opencv2/opencv.hpp>
#include <opencv2/geometry.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdint>
#include "third_party/Doxa/Doxa/Sauvola.hpp"

cv::Mat binarizeWithDoxa(const cv::Mat& input);

namespace {
    constexpr double MIN_AREA_RATIO = 0.10;
    constexpr double APPROX_RATIO = 0.02;
    constexpr double CANNY_LOW = 50;
    constexpr double CANNY_HIGH = 150;

    // 중심을 기준으로 회전 순서로 정렬하고 TL부터 시작한다.
    // 결과: TL, TR, BR, BL
    std::array<cv::Point2f, 4> orderPoints(
        const std::vector<cv::Point>& points)
    {
        std::array<cv::Point2f, 4> ordered;
        cv::Point2f center(0, 0);

        for (int i = 0; i < 4; ++i) {
            ordered[i] = cv::Point2f(
                static_cast<float>(points[i].x),
                static_cast<float>(points[i].y));
            center += ordered[i];
        }
        center *= 0.25f;

        std::sort(ordered.begin(), ordered.end(),
            [center](const cv::Point2f& a, const cv::Point2f& b) {
                return std::atan2(a.y - center.y, a.x - center.x)
                    < std::atan2(b.y - center.y, b.x - center.x);
            });

        auto topLeft = std::min_element(
            ordered.begin(), ordered.end(),
            [](const cv::Point2f& a, const cv::Point2f& b) {
                return a.x + a.y < b.x + b.y;
            });

        std::rotate(ordered.begin(), topLeft, ordered.end());
        return ordered;
    }
}

bool detectDocument(
    const cv::Mat& input,
    std::array<cv::Point2f, 4>& corners,
    cv::Mat& gray,
    cv::Mat& blurred,
    cv::Mat& edges,
    cv::Mat& contourView)
{
    if (input.empty() || input.cols < 32 || input.rows < 32)
        return false;

    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);

    const double scale = std::min(
        1.0, 1000.0 / std::max(input.cols, input.rows));

    cv::Mat smallGray;
    cv::resize(gray, smallGray, cv::Size(), scale, scale, cv::INTER_AREA);

    cv::GaussianBlur(smallGray, blurred, cv::Size(5, 5), 0);
    cv::Canny(blurred, edges, CANNY_LOW, CANNY_HIGH);

    // 가까이 끊어진 테두리를 연결
    const cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(5, 5));

    cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, kernel);

    // 원본 크기로 복원
    cv::resize(
        edges, edges, input.size(), 0, 0, cv::INTER_NEAREST);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        edges.clone(), contours,
        cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    contourView = input.clone();
    cv::drawContours(
        contourView, contours, -1, cv::Scalar(0, 180, 0), 1);

    std::sort(contours.begin(), contours.end(),
        [](const auto& a, const auto& b) {
            return cv::contourArea(a) > cv::contourArea(b);
        });

    const double minArea =
        static_cast<double>(input.total()) * MIN_AREA_RATIO;

    for (const auto& contour : contours) {
        if (cv::contourArea(contour) < minArea)
            break;

        std::vector<cv::Point> polygon;
        cv::approxPolyDP(
            contour, polygon,
            APPROX_RATIO * cv::arcLength(contour, true), true);

        if (polygon.size() != 4 || !cv::isContourConvex(polygon))
            continue;

        const auto candidate = orderPoints(polygon);
        bool valid = true;

        for (int i = 0; i < 4; ++i) {
            if (cv::norm(candidate[i] - candidate[(i + 1) % 4]) < 20)
                valid = false;
        }

        if (valid) {
            corners = candidate;
            return true;
        }
    }

    return false;
}

cv::Mat correctPerspective(
    const cv::Mat& input,
    const std::array<cv::Point2f, 4>& corners)
{
    if (input.empty())
        return {};

    const int width = static_cast<int>(std::ceil(std::max(
        cv::norm(corners[1] - corners[0]),
        cv::norm(corners[2] - corners[3]))));

    const int height = static_cast<int>(std::ceil(std::max(
        cv::norm(corners[3] - corners[0]),
        cv::norm(corners[2] - corners[1]))));

    if (width < 20 || height < 20)
        return {};

    const std::array<cv::Point2f, 4> destination = {
        cv::Point2f(0, 0),
        cv::Point2f(static_cast<float>(width - 1), 0),
        cv::Point2f(
            static_cast<float>(width - 1),
            static_cast<float>(height - 1)),
        cv::Point2f(0, static_cast<float>(height - 1))
    };

    const cv::Mat transform = cv::getPerspectiveTransform(
        corners.data(), destination.data());

    cv::Mat corrected;
    cv::warpPerspective(
        input, corrected, transform, cv::Size(width, height));

    return corrected;
}

void showImage(const std::string& title, const cv::Mat& image)
{
    if (image.empty())
        return;

    cv::namedWindow(title, cv::WINDOW_NORMAL);
    cv::imshow(title, image);

    const double scale = std::min(
        900.0 / image.cols, 650.0 / image.rows);

    cv::resizeWindow(
        title,
        std::max(1, static_cast<int>(image.cols * scale)),
        std::max(1, static_cast<int>(image.rows * scale)));
}

int main()
{
    const std::string folder = "D:/DocumentScanner/";
    const cv::Mat input = cv::imread(folder + "test.jpg");

    if (input.empty()) {
        std::cerr << "Cannot open D:/DocumentScanner/test.jpg\n";
        return 1;
    }

    try {
        std::array<cv::Point2f, 4> corners;
        cv::Mat gray, blurred, edges, contourView;

        const bool found = detectDocument(
            input, corners, gray, blurred, edges, contourView);

        showImage("1 - Original", input);
        showImage("2 - Canny edges", edges);

        cv::imwrite(folder + "debug_gray.jpg", gray);
        cv::imwrite(folder + "debug_blurred.jpg", blurred);
        cv::imwrite(folder + "debug_edges.png", edges);
        cv::imwrite(folder + "debug_contours.jpg", contourView);

        if (!found) {
            std::cout << "Document not found. Check Canny edges.\n";
            cv::waitKey(0);
            return 0;
        }

        cv::Mat marked = input.clone();
        const char* labels[] = { "TL", "TR", "BR", "BL" };

        for (int i = 0; i < 4; ++i) {
            const cv::Point p(
                cvRound(corners[i].x), cvRound(corners[i].y));
            const cv::Point next(
                cvRound(corners[(i + 1) % 4].x),
                cvRound(corners[(i + 1) % 4].y));

            cv::line(marked, p, next, cv::Scalar(0, 255, 0), 4);
            cv::circle(marked, p, 10, cv::Scalar(0, 0, 255), -1);
            cv::putText(
                marked, labels[i], p + cv::Point(10, 25),
                cv::FONT_HERSHEY_SIMPLEX, 0.8,
                cv::Scalar(0, 0, 255), 2);
        }

        const cv::Mat corrected = correctPerspective(input, corners);

        showImage("3 - Detected document", marked);

        if (corrected.empty()) {
            std::cout << "Invalid document size.\n";
        }
        else {
            showImage("4 - Corrected document", corrected);
            // Doxa의 Sauvola 알고리즘으로 흑백 이진화
            const cv::Mat scan = binarizeWithDoxa(corrected);

            if (!scan.empty()) {
                showImage("5 - Doxa Sauvola scan", scan);

                if (cv::imwrite(folder + "scan_doxa.png", scan))
                    std::cout << "Saved: D:/DocumentScanner/scan_doxa.png\n";
            }
            cv::imwrite(folder + "detected.jpg", marked);

            if (cv::imwrite(folder + "corrected.jpg", corrected))
                std::cout << "Saved: D:/DocumentScanner/corrected.jpg\n";
        }

        cv::waitKey(0);
    }
    catch (const cv::Exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}

cv::Mat binarizeWithDoxa(const cv::Mat& input)
{
    if (input.empty())
        return {};

    cv::Mat gray;

    if (input.type() == CV_8UC3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }
    else if (input.type() == CV_8UC1) {
        gray = input.clone();
    }
    else {
        return {};
    }

    if (!gray.isContinuous())
        gray = gray.clone();

    const Doxa::Image source = Doxa::Image::Reference(
        gray.cols,
        gray.rows,
        gray.ptr<Doxa::Pixel8>()
    );

    // 초기 시험값: 주변 영역 크기와 Sauvola 계수
    const Doxa::Parameters parameters({
        {"window", 31},
        {"k", 0.2}
        });

    const Doxa::Image binary =
        Doxa::Sauvola::ToBinaryImage(source, parameters);

    return cv::Mat(
        binary.height,
        binary.width,
        CV_8UC1,
        binary.data
    ).clone();
}
