
// use if opencv ws compiled?
//#include <cv.h>
//#include <highgui.h>
//#include <imgproc.hpp>


#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/objdetect/objdetect.hpp>
//#define DEBUG

#include <stdint.h>
#include <string>
#include <iostream>
#include <exception>
#include <stdio.h>

using std::cout;
using std::endl;
using cv::Mat;



#define DOWNSCALE 1.5

void drawOptFlowMap(const cv::Mat& flow, cv::Mat& cflowmap, int step, double scale, const cv::Scalar& color) {
    for (int y = 0; y < cflowmap.rows; y += step)
        for (int x = 0; x < cflowmap.cols; x += step) {
            const cv::Point2f& fxy = flow.at<cv::Point2f>(y, x);
            cv::line(cflowmap, cv::Point(x, y), cv::Point(cvRound(x+fxy.x), cvRound(y+fxy.y)), color);
        }
}

// get next image from video, 24hz
// get motion ratio
// compare motion ratio
// 	if > threshold then 
// 		get foreground, modify and display on screen
// 	if not, then slirgly update background levels.
// detect faces, do stuff on it.


void lin_serial_send(void* pdata, unsigned int size);
int lin_serial_connect(char*);

int main( int argc, char** argv ) {

    lin_serial_connect("/dev/ttyUSB0");
    uint16_t res[16];

    Mat trollFace = cv::imread("trollface.png", 0);

    Mat frame, miniFrame, previousFrame, flow;
    cv::VideoCapture cap(CV_CAP_ANY); // open the default camera
    if (!cap.isOpened()) {
        cout << "unable to open camera 0" << endl;
        cap = cv::VideoCapture(1); // open the default camera
        if (!cap.isOpened()) {
            cout << "unable to open camera 1" << endl;
            return -1;
        }
    }

    cap >> frame;
    cv::cvtColor(frame, frame, CV_BGR2GRAY);

    cv::CascadeClassifier haar_cascade;
    haar_cascade.load("haarcascade_frontalface_default.xml");
    std::vector<cv::Rect> faces;

    float aspectRatio = 1.0 * static_cast<float>(frame.cols) / static_cast<float>(frame.rows);
    int frameRateVal = 100;
    int resizeVal = frame.rows - 16;
    int blurVal = 10;
    int cannyVal = 34;
    int thresholdVal = 0;
    int motionVal = 0;
    int facesVal = 0;

    cv::namedWindow("playground", CV_WINDOW_NORMAL);
    cv::namedWindow("sliders", CV_WINDOW_NORMAL);
    cv::createTrackbar("frame rate", "sliders", &frameRateVal, 100);
    cv::createTrackbar("resize", "sliders", &resizeVal, frame.rows - 16);
    cv::createTrackbar("gaussian blur", "sliders", &blurVal, 50);
    cv::createTrackbar("edge detection", "sliders", &cannyVal, 150);
    cv::createTrackbar("adaptive threshold", "sliders", &thresholdVal, 50);
    cv::createTrackbar("motion sensitivity", "sliders", &motionVal, 50); // TODO make sensitivity thres on vectors.
    cv::createTrackbar("face detection", "sliders", &facesVal, 3); // TODO make sensitivity thres on vectors.

    while (true) {
        cv::Mat processed;
        previousFrame = frame;
        cap >> frame; // get a new frame from camera
        cv::cvtColor(frame, frame, CV_BGR2GRAY);
        cv::resize(frame, miniFrame, cv::Size(frame.cols/DOWNSCALE, frame.rows/DOWNSCALE));
        processed = frame;

        if (resizeVal != 0) {
            if (resizeVal < 16) {
                resizeVal = 16;
            }
            std::cout << "aspectRatio " << aspectRatio << "  resizeVal: " << resizeVal << " resizeVal*aspectRatio" << resizeVal*aspectRatio << std::endl;
            cv::resize(processed, processed, cv::Size((resizeVal+16)*(aspectRatio), resizeVal+16));
        }

        if (blurVal != 0) {
            cv::GaussianBlur(processed, processed, cv::Size(7,7), blurVal/10., blurVal/10.);
        }

        if (cannyVal != 0) {
            cv::Canny(processed, processed, 0, cannyVal, 3);
        }
        if (thresholdVal !=0) {
            cv::adaptiveThreshold(processed, processed, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 19, thresholdVal);
        }
        if (motionVal != 0 && frame.size() == previousFrame.size()) {
            if (frame.elemSize() != 1) {
                cv::cvtColor(frame, frame, CV_BGR2GRAY);
            }
            if (previousFrame.channels() != 1) {
                cv::cvtColor(previousFrame, previousFrame, CV_BGR2GRAY);
            }
            cv::calcOpticalFlowFarneback(frame, previousFrame, flow, 0.5, 3, 5, 1, 5, 1.2, 0);
            flow = cv::abs(flow);
            cv::threshold(flow, flow, motionVal, 0, cv::THRESH_TOZERO);  // FIXME
            cv::Scalar res = cv::sum(flow);
            std::cout << "flow total : " << res.val[0] + res.val[1] << std::endl;
            cv::cvtColor(processed, processed, CV_GRAY2RGB);
            drawOptFlowMap(flow, processed, 6, 1.5, CV_RGB(0, 255, 0));
        }
        if (facesVal != 0) {
            if (frame.elemSize() == 1) { // switch to color.
                cv::cvtColor(frame, frame, CV_GRAY2BGR);
            }
            haar_cascade.detectMultiScale(miniFrame, faces);
            Mat resizedTroll, roi;
            cv::Rect headSize;
            for (std::vector<cv::Rect>::const_iterator it = faces.begin(); it != faces.end(); ++it) {
                headSize = cv::Rect(it->x*DOWNSCALE, it->y*DOWNSCALE, it->width*DOWNSCALE, it->height*DOWNSCALE);
                if (facesVal == 1) {
                    rectangle(processed, headSize, CV_RGB(0, 255,0), 2);
                } else if (facesVal ==2) {
                    roi = processed(cv::Range(it->x * DOWNSCALE, (it->x + it->width) * DOWNSCALE), cv::Range(it->y, (it->y + it->height) * DOWNSCALE));
                    cv::resize(trollFace, resizedTroll, cv::Size(headSize.width, headSize.height));
                    cout << headSize.x << " : " << headSize.y << "  size " << roi.cols << " " << roi.rows << endl;
                    
                    roi = Mat::ones(cv::Size(10, 10), CV_8UC1) * 255;
                    // put a trollface
                } else if (facesVal ==3) {
                    // put a fuuuuuu
                }
            }
        }
        // TODO background adaptive filter
        //

        // TODO: train a face live?
        // Methodo: get face. get images from face. rotate and crop. re-train the algo.

        cv::imshow("playground", processed);

        for (int i = 0; i < 16; i++) {
            res[i] =0;
        }

        // TODO function for net Output.
        cv::resize(processed, processed, cv::Size(16,16));
        int i = 0;
        for (cv::MatConstIterator_<uint8_t> it = processed.begin<uint8_t>(); it != processed.end<uint8_t>(); ++it) {
            for (int j = 0; j < processed.cols; j++) {
                //cout << " "<< static_cast<uint16_t>(*it);
                if (*it > 1) {
                    res[i] |= (0b1 << j) ;
                }
                ++it;
            }
            //cout << endl;
            i++;
        }
        lin_serial_send(res, 32);

        for (int i =0; i < 16; i++) {
            //std::cout << res[i] << std::endl;
            for (int j =0; j < 16; j++) {
                printf(" %d", (res[i] & (0b1 << j)) >> j);
            }
            cout << endl;
        }
        cout << endl << endl;

        if(cv::waitKey(100 - frameRateVal+1) >= 0) {
        }
    }
}


