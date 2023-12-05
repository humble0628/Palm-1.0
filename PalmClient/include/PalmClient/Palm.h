#ifndef _PALM_H
#define _PALM_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <chrono>
#include <thread>

#include <opencv2/opencv.hpp>
#include <Eigen/Dense>
#include <boost/asio.hpp>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

#include "AESCoder.h"
#include "License.h"
#include "Camera.h"
#include "Net.h"
#include "ConstVar.h"


class Client;

// 用户data
struct UserData
{
public:
    std::string name;      // 用户名
    std::vector<float> feature; // 特征向量
};

/*
类名：CPalm
入口：CLicense类 激活标志位
     CCamera类 ir_frame
     CNet类特征向量 roi标志位
出口：注册
     识别
     数据库操作
     退出
     绘图
*/
class CPalm
{
public:
    void userRegister();
    void userRecognize();
    void userDatabase();
    void userQuit();
    const cv::Mat getFrameWithPoints();
    CPalm(boost::asio::io_context& ioc);
    ~CPalm();
private:
    bool usernameCheckCB(std::string& username);
    Json::Value featureMatchCB(std::vector<float>& feature);
    bool databaseUpdateCB(std::string& username, std::vector<float>& feature);
    bool usernameCheck(std::string& username);
    bool assessImg(cv::Mat& img);
    void membersClear();
private:
    // Camera接口变量
    CCamera _camera;                               // Camera对象
    cv::Mat _ir_frame;                             // ir原始图像

    // Net接口变量
    CNet _cnet;                                    // CNet对象
    cv::Mat _roi_img;                              // roi图像
    bool _roi_fail;                                // roi获取失败标志位 true代表获取失败
    std::vector<cv::Point> _points;                // 关键点
    std::vector<cv::Point> _roi_rect;              // roi矩形四个顶点

    // Palm自有成员属性
    static const int IMG_NUM_REGISTER = 5;          // 注册需要的有效图像数量
    static const int IMG_NUM_RECOGNIZE = 2;         // 识别需要的有效图像数量
    static const int PALM_COUNT = 2000;             // 手掌检测轮数
    static const int ROI_COUNT = 1000;              // roi检测轮数
    static const int LEN_FEATURE = 128;             // 特征向量长度   C++11对静态常量的声明方式，不需要在类外初始化

    bool _palm_fail;                               // 检测手掌标志位
    bool _img_scalar_fail;                         // 图像平均曝光 即进行手掌检测时图像的曝光
    
    UserData _user;                                // 包含用户名与特征向量
    std::vector<std::vector<float>> _features;     // 储存有效的特征

    std::shared_ptr<Client> _client;
};

#endif