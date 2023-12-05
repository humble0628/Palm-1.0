#include "Camera.h"

CCamera::CCamera() {
    _ir_cap.open(2);
    if (!_ir_cap.isOpened()) {
        std::cerr << "\nIR摄像头打开失败,请检查" << std::endl;
        exit(-1);   // 程序终止状态码为 -1
    }
	_ir_cap.set(cv::CAP_PROP_FPS,30);
	_ir_cap.set(cv::CAP_PROP_CONTRAST, 45);
	_ir_cap.set(cv::CAP_PROP_GAMMA, 90);
	_ir_cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('Y', 'U', 'V', '2'));
	_ir_cap.set(cv::CAP_PROP_FRAME_WIDTH,640);
	_ir_cap.set(cv::CAP_PROP_FRAME_HEIGHT,480);
    _ir_cap >> _ir_frame;     // 读取一帧图像 用作初始化 ir_frame
    cv::transpose(_ir_frame, _ir_frame);
    cv::flip(_ir_frame, _ir_frame, 0);

    _rgb_cap.open(0);
    if (!_ir_cap.isOpened()) {
        std::cerr << "\nRGB摄像头打开失败,请检查" << std::endl;
        exit(-1);   // 程序终止状态码为 -1
    }
    _rgb_cap.set(cv::CAP_PROP_FPS,30);
	_rgb_cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
	_rgb_cap.set(cv::CAP_PROP_FRAME_WIDTH,640);
	_rgb_cap.set(cv::CAP_PROP_FRAME_HEIGHT,480);
    _rgb_cap >> _rgb_frame;   // 读取一帧图像 用作初始化 rgb_frame
    cv::transpose(_rgb_frame, _rgb_frame);
    cv::flip(_rgb_frame, _rgb_frame, 0);

    std::cout << "\n摄像头初始化完成" << std::endl;
}

CCamera::~CCamera() {
    // 释放摄像头
    _ir_cap.release(); _rgb_cap.release();
    cv::destroyAllWindows();
}

// 获取 Mat 对象 frame, 得到一帧图像
cv::Mat& CCamera::getFrame(std::string id) {
    if (id == "ir") {
        if (!_ir_cap.isOpened()) {
            std::cerr << "\nIR摄像头未打开,请检查" << std::endl;
            exit(-1);   // 程序终止状态码为 -1
        }
        if (_ir_frame.empty()) {
            std::cerr << "\nIR摄像头无图像" << std::endl;
            exit(-2);   // 程序终止状态码为 -2
        }
        _ir_cap >> _ir_frame;
        cv::transpose(_ir_frame, _ir_frame);
        cv::flip(_ir_frame, _ir_frame, 0);
        return _ir_frame;
    }
    else if (id == "rgb") {
        if (!_rgb_cap.isOpened()) {
            std::cerr << "\nRGB摄像头未打开,请检查" << std::endl;
            exit(-1);   // 程序终止状态码为 -1
        }
        if (_rgb_frame.empty()) {
            std::cerr << "\nRGB摄像头无图像" << std::endl;
            exit(-2);   // 程序终止状态码为 -2
        }
        _rgb_cap >> _rgb_frame;
        cv::transpose(_rgb_frame, _rgb_frame);
        cv::flip(_rgb_frame, _rgb_frame, 0);
        return _rgb_frame;
    } else {
        std::cerr << "\nid输入错误" << std::endl;
        exit(-3);   // 程序终止状态码为 -3
    }
}

cv::Mat& CCamera::getFrame(std::string id, std::string) {
    if (id == "ir") {
        return _ir_frame;
    }
    else if (id == "rgb") {
        return _rgb_frame;
    }
    else {
        std::cerr << "\nid输入错误" << std::endl;
        exit(-3);   // 程序终止状态码为 -3
    }
}