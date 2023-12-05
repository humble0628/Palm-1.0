#ifndef _NET_H
#define _NET_H

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <typeinfo>

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

#include "AESCoder.h"

#define PI 3.14159265

/*
类名：CNet
入口：CCamera类 ir_frame
     AESCoder类 解密方法
出口：获取roi图以及标志位
     获取特征向量
     获取关键点
*/
class CNet
{
public:
    bool palmDetect(cv::Mat& ir_frame);
    bool roiDetect(cv::Mat& ir_frame);
    std::vector<float> getFeature();
    const cv::Mat& getRoiImg() const;
    const bool& getRoiFail() const;
    const std::vector<cv::Point>& getPoints() const;
    const std::vector<cv::Point>& getRect() const;
    CNet();
    ~CNet();
private:
    bool decryptModel(std::string model_path, std::vector<uchar>& model_vec);
    void readModel();
    std::vector<cv::String> getOutlayerNames(cv::dnn::Net net);
    void getYoloOutput(cv::Mat& ir_frame);
    void extractRoi(cv::Mat& ir_frame, float coordinate[3][2]);
    std::vector<float> normalize(const std::vector<float>& code);
    cv::Mat& addChannels(cv::Mat& img);
private:
    cv::Mat _roi_img;                  // roi图像
    bool _roi_fail;                    // roi获取失败标志位 true代表获取失败
    std::vector<float> _feature;       // 特征向量
    std::vector<cv::Point> _points;    // 关键点坐标 两个手指和手掌的中心坐标
    std::vector<cv::Point> _roi_rect;  // roi矩形四个顶点

    float _conf_thres;                 // 置信度阈值
    float _nms_thres;                  // 非极大抑制阈值

    int _init_img_size;                // 原始图像大小
    int _roi_img_size;                 // roi图像大小

    cv::Mat _blob_roi;                 // roi模型输入                 
    std::vector<cv::Mat> _outs;        // roi模型输出
    cv::Mat _blob_feature;             // feature模型输入

    cv::dnn::Net _roi_net, _feature_net;
    std::string _roi_define_path, _roi_model_path, _feature_model_path;
    std::vector<uchar> _roi_define_vec, _roi_net_vec, _feature_net_vec;
};

#endif