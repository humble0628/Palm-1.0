#include "Net.h"

// 初始化各种参数 读取模型
CNet::CNet() {
    _conf_thres = 0.5;
    _nms_thres = 0.4;

    _init_img_size = 416;
    _roi_img_size = 112;

    _roi_define_path = R"(../../res/model.c)";
    _roi_model_path = R"(../../res/weight.w)";
    _feature_model_path = R"(../../res/model.p)";

    readModel();

    _roi_img = cv::Mat::zeros(_roi_img_size, _roi_img_size, CV_8U);
    _roi_fail = true;
    _feature = std::vector<float>(128, 0.0);
    _points = std::vector<cv::Point>(3, cv::Point(0.0, 0.0));
}

CNet::~CNet() {

}

// 对模型进行解密 成功返回true 否则false
bool CNet::decryptModel(std::string model_path, std::vector<uchar>& model_vec) {
	std::ifstream fin(model_path);
	if (!fin.is_open()) return false;

	std::string ciphertext((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
	fin.close();

	// 解密被加密的字符串
	std::string decryptedtext = AESCoder::aesDecrypt(ciphertext);

	model_vec.assign(decryptedtext.begin(), decryptedtext.end());
	copy(decryptedtext.begin(), decryptedtext.end(), model_vec.begin());
    return true;
}

// 读取并设置模型推理参数 在构造函数中使用
void CNet::readModel() {
    // 先进行模型解密
    bool decrypt_define_label = decryptModel(_roi_define_path, _roi_define_vec);
    bool decrypt_roi_label = decryptModel(_roi_model_path, _roi_net_vec);
    bool decrypt_feature_label = decryptModel(_feature_model_path, _feature_net_vec);

    if (!(decrypt_define_label && decrypt_roi_label && decrypt_feature_label)) {
        std::cerr << "\n模型解密失败" << std::endl;
        exit(-4);
    }
    std::cerr << "\n模型加载完成" << std::endl;

    // 解密完成后对两个模型进行读取 并设置
    _roi_net = cv::dnn::experimental_dnn_34_v22::readNetFromDarknet(_roi_define_vec, _roi_net_vec);
    _roi_net.setPreferableBackend(cv::dnn::experimental_dnn_34_v22::Backend::DNN_BACKEND_OPENCV);
    _roi_net.setPreferableTarget(cv::dnn::experimental_dnn_34_v22::Target::DNN_TARGET_CPU);

    _feature_net = cv::dnn::experimental_dnn_34_v22::readNetFromTensorflow(_feature_net_vec);
}

// 获取模型所有输出层名 用作前向预测形参 
// XXX：不懂为什么不用默认参数
std::vector<cv::String> CNet::getOutlayerNames(cv::dnn::Net net) {
    std::vector<cv::String> names;
    if (names.empty()) {
        //得到输出层索引号
        std::vector<int> outLayers = net.getUnconnectedOutLayers();
 
        //得到网络中所有层名称
        std::vector<cv::String> layersNames = net.getLayerNames();

        //获取输出层名称
        names.resize(outLayers.size());
        for (int i = 0; i < outLayers.size(); ++i)
            names[i] = layersNames[outLayers[i] - 1];
    }
    return names;
}

// opencv进行模型推理 得到推理结果outs
void CNet::getYoloOutput(cv::Mat& ir_frame) {
    // 将原始图像进行处理得到模型输入需要的格式  进行了颜色变换 bgr->rbg
    cv::dnn::blobFromImage(ir_frame, _blob_roi, 1 / 255.0, cv::Size(_init_img_size, _init_img_size), cv::Scalar(0, 0, 0), true, false);
    _roi_net.setInput(_blob_roi);

    // 前向预测得到输出
    _roi_net.forward(_outs, this->getOutlayerNames(_roi_net));
}

// 利用yolo检测到的手掌与关键点建立坐标系 得到roi图像
// XXX：角度接近90或-90时出现错误
void CNet::extractRoi(cv::Mat& ir_frame, float coordinate[3][2]) {
    cv::Mat gray_img;
	cv::cvtColor(ir_frame, gray_img, CV_BGR2GRAY);
	
	int W = gray_img.cols;
	int H = gray_img.rows;

    // (x1, y1) (x2, y2)为手指中间的两个点  (x3, y3)为手掌中心点
	float x1 = coordinate[1][0];
	float y1 = coordinate[1][1];
	float x2 = coordinate[2][0];
	float y2 = coordinate[2][1];
	float x3 = coordinate[0][0];
	float y3 = coordinate[0][1];

	float x4, y4;      // ROI四个角点中下边两个点其中一个
	float x5, y5;      // ROI四个角点中上边两个点其中一个
	float k2, b2;      

	float x0 = (x1 + x2) / 2;
	float y0 = (y1 + y2) / 2;
	
	float unit_len = sqrt(pow((x2 - x1), 2) + pow((y2 - y1), 2));
	float k1 = (y1 - y2) / ((x1 - x2) + 1e-9);
	float b1 = y1 - k1 * x1;

	if (y1 == y2) {
		x4 = x0;
		y4 = y0 + unit_len/2;
        k2 = 0;
	} else {
		k2 = (-1) / (k1 + 1e-9);
	    b2 = y0 - k2 * x0;
		
		float aa1 = 1 + pow(k2, 2);;
		float bb1 = 2 * (-x0 + k2 * b2 - k2 * y0);
		float cc1 = pow((x0), 2) + pow((b2 - y0), 2) - pow(unit_len, 2) / 4;
		float temp = sqrt(pow((bb1), 2) - 4 * aa1 * cc1);
		float x4_1 = (-bb1 + temp) / (2 * aa1);
		float y4_1 = k2 * x4_1 + b2;
		float x4_2 = (-bb1 - temp) / (2 * aa1);
		float y4_2 = k2 * x4_2 + b2;

        // 将x4赋值为 使roi尽可能小的点
		if ((pow((x3 - x4_1), 2) + pow((y3 - y4_1), 2)) < (pow((x3 - x4_2), 2) + pow((y3 - y4_2), 2))) {
			x4 = x4_1;
			y4 = y4_1;
		} else {
			x4 = x4_2;
			y4 = y4_2;
		}
	}

	float b4 = y4 - k1 * x4;
	float aa2= 1 + pow(k1, 2);
	float bb2= 2 * (-x4 + k1 * b4 - k1 * y4);
	float cc2 = pow((x4), 2) + pow((b4 - y4), 2) - pow(unit_len, 2);
	
	float x5_1 = (-bb2 + sqrt(pow((bb2), 2) - 4 * aa2 * cc2)) / (2 * aa2);
	float y5_1 = k1 * x5_1 + b4;
	float x5_2 = (-bb2 - sqrt(pow((bb2), 2) - 4 * aa2 * cc2)) / (2 * aa2);
	float y5_2 = k1 * x5_2 + b4;

    // 将x5赋值为 使roi尽可能小的点
	if (x5_1 < x5_2) {
		x5 = x5_1;
		y5 = y5_1;
	} else {
		x5 = x5_2;
		y5 = y5_2;
	}
	if (y1 == y2) {
		cv::Mat roi = gray_img(cv::Rect(int(x5), int(y5) , int(unit_len * 2), int(unit_len * 2)));
		cv::resize(roi, _roi_img, cv::Size(_roi_img_size, _roi_img_size), 0, 0, CV_INTER_CUBIC);

        // 计算矩形的四个顶点，该条件不涉及旋转
        cv::Point topRight(x5 + unit_len * 2, y5);
        cv::Point bottomRight(x5 + unit_len * 2, y5 + unit_len * 2);
        cv::Point bottomLeft(x5, y5 + unit_len * 2);
        _roi_rect = {cv::Point(x5, y5), topRight, bottomRight, bottomLeft};

        _roi_fail = false;
	} else {
		float angle = atan(1 / k2);
		float angle_degree = -angle * 180 / PI;
		cv::Point2f center(W / 2, H / 2);
		cv::Mat M(2, 3, CV_32FC1);
		M = getRotationMatrix2D(center, angle_degree, 1);           // 旋转矩阵
		cv::warpAffine(gray_img, gray_img, M, cv::Size(W, H));      // 仿射变换——这里为旋转
		
		int new_x5 = static_cast<int>(x5*std::cos(angle) - y5*std::sin(angle) - (W/2)*std::cos(angle) + (H/2)*std::sin(angle) + W/2);
        int new_y5 = static_cast<int>(x5*std::sin(angle) + y5*std::cos(angle) - (W/2)*std::sin(angle) - (H/2)*std::cos(angle) + H/2);
		if (new_x5 < 0 || new_y5 < 0 || (new_x5 + unit_len * 2 > W) || (new_y5 + unit_len * 2 > H)) {
            _roi_img = cv::Mat::zeros(_roi_img_size, _roi_img_size, CV_8UC1);
            _roi_fail = true;
        } else {
			cv::Mat roi = gray_img(cv::Rect(new_x5, new_y5 , int(unit_len * 2), int(unit_len * 2)));
			cv::resize(roi, _roi_img, cv::Size(_roi_img_size, _roi_img_size), 0, 0, CV_INTER_CUBIC);

            // 计算旋转前的矩形四个顶点
            cv::Point topRight(x5 + unit_len * 2 * std::cos(-angle), y5 + unit_len * 2 * std::sin(-angle));
            cv::Point bottomRight(topRight.x - unit_len * 2 * std::sin(-angle), topRight.y + unit_len * 2 * std::cos(-angle));
            cv::Point bottomLeft(x5 - unit_len * 2 * std::sin(-angle), y5 + unit_len * 2 * std::cos(-angle));
            _roi_rect = {cv::Point(x5, y5), topRight, bottomRight, bottomLeft};

            _roi_fail = false;
		}
	}
}

// 特征归一化
std::vector<float> CNet::normalize(const std::vector<float>& code) {
    std::vector<float> normalized_code(code.size());

    float norm = 0.0;
    for (const float& value : code) {
        norm += value * value;
    }
    norm = std::sqrt(norm);
    // 避免2-范数为0
    const float epsilon = 1e-5;
    norm = std::max(norm, epsilon);

    for (int i = 0; i < code.size(); ++i) {
        normalized_code[i] = code[i] / norm;
    }

    return normalized_code;
}

// 将单通道灰度图转化成三通道灰度图 三个通道相同
cv::Mat& CNet::addChannels(cv::Mat& img) {
    cv::Mat temp_img(img.rows, img.cols, CV_8UC3);
    cv::Mat channels[] = {img, img, img};
    cv::merge(channels, 3, img);
    return img;
}

//***************************接口函数****************************
// 手掌检测，如果训练一个轻量的手掌检测网络会更好
// 检测到手掌返回 false
bool CNet::palmDetect(cv::Mat& ir_frame)
{
    // 首先推理yolo得到 _outs
    getYoloOutput(ir_frame);

    // 首先遍历每一个检测对象
    for (const cv::Mat& outs : this->_outs) {
        // 随后遍历每个对象的所有检测框 每一行都是一个检测框 [x, y, w, h, __, 手指关键点置信度, 手掌关键点置信度]
        for (int i = 0; i < outs.rows; ++i) {
            cv::Mat detection = outs.row(i);
            cv::Mat scores = detection.colRange(5, outs.cols);
			cv::Point classIdPoint;
			double confidence;
            // 该函数需要 double 数据类型
			cv::minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);

            // 初次依据置信度筛选
			if (static_cast<float>(confidence) > _conf_thres) {
                if (classIdPoint.x == 1) return false;
            }
        }
    }
    return true;
}

// roi检测+提取核心程序 返回roi提取是否成功 即_roi_fail标志位
bool CNet::roiDetect(cv::Mat& ir_frame) {   
    // 局部变量定义
    float coordinate[3][2];     // 关键点坐标 两个手指和手掌的中心坐标
    int dfg = 0, pc = 0;        // dfg 为手指关键点个数， pc为手掌关键点个数
	std::vector<int> classIds;
	std::vector<float> confidences;
	std::vector<cv::Rect> boxes;

    // 首先推理yolo得到 _outs
    getYoloOutput(ir_frame);

    // 首先遍历每一个检测对象
    for (const cv::Mat& outs : this->_outs) {
        // 随后遍历每个对象的所有检测框 每一行都是一个检测框 [x, y, w, h, __, 手指关键点置信度, 手掌关键点置信度]
        for (int i = 0; i < outs.rows; ++i) {
            cv::Mat detection = outs.row(i);
            cv::Mat scores = detection.colRange(5, outs.cols);
			cv::Point classIdPoint;
			double confidence;
            // 该函数需要 double 数据类型
			cv::minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);

            // 初次依据置信度筛选
			if (static_cast<float>(confidence) > _conf_thres) {
				int centerX = static_cast<int>(detection.at<float>(0, 0) * ir_frame.cols);
				int centerY = static_cast<int>(detection.at<float>(0, 1) * ir_frame.rows);
				int width = static_cast<int>(detection.at<float>(0, 2) * ir_frame.cols);
				int height = static_cast<int>(detection.at<float>(0, 3) * ir_frame.rows);
				int left = static_cast<int>(centerX - width / 2);
				int top = static_cast<int>(centerY - height / 2);

				classIds.push_back(classIdPoint.x);
				confidences.push_back(static_cast<float>(confidence));
				boxes.push_back(cv::Rect(left, top, width, height));
            }
        }
    }

    // nms非极大抑制 二次依据抑制阈值筛选
	std::vector<int> indices;
    // 这里需要 float 类型 
	cv::dnn::NMSBoxes(boxes, confidences, _conf_thres, _nms_thres, indices);

    // 获取筛选后的关键点坐标
    for (const int& idx : indices) {
        cv::Rect box = boxes[idx];
        if (pc == 1 && dfg == 2) break;

        // 手掌的label = 1
        if (classIds[idx] == 1 && pc < 1) {
            coordinate[pc][0] = box.x + box.width / 2;
            coordinate[pc][1] = box.y + box.height / 2;
            _points[pc] = cv::Point(coordinate[pc][0], coordinate[pc][1]);
            ++pc;
        }
        // 手指 label
        else if (classIds[idx] != 1 && dfg < 2) {
            coordinate[dfg+1][0] = box.x + box.width / 2;
            coordinate[dfg+1][1] = box.y + box.height / 2;
            _points[dfg+1] = cv::Point(coordinate[dfg+1][0], coordinate[dfg+1][1]);
            ++dfg;
        }
    }
    //********** 手掌有无判断 *********
    // 没有检测到三个关键点
    if (pc != 1 || dfg != 2) {
        this->_roi_fail = true;
        this->_roi_img = cv::Mat::zeros(_roi_img_size, _roi_img_size, ir_frame.type());
        _points = std::vector<cv::Point>(3, cv::Point(0.0, 0.0));
    }
    // 检测到三个关键点
    // XXX：距离筛选不合理
    else {
        // 距离筛选
        float dis = sqrt(pow((coordinate[2][0] - coordinate[1][0]), 2) + pow((coordinate[2][1] - coordinate[1][1]), 2));
        if (dis > 80) {
			// std::cout << "\n手掌放置位置太近，请将手掌放至距离摄像头15-25cm处！\n" << std::endl;
            _roi_fail = true;
		}
        else if (dis < 50) {
			// std::cout << "\n手掌放置位置太远，请将手掌放至距离摄像头15-25cm处！\n" << std::endl;
            _roi_fail = true;
        }
        // 距离合格
        else {
            extractRoi(ir_frame, coordinate);

            // roi初步提取成功
            if (!_roi_fail) {
                // 曝光筛选
                cv::Scalar gray_scalar = cv::mean(_roi_img);
                if (gray_scalar.val[0] > 250 || gray_scalar.val[0] < 90) {
                    _roi_fail = true;
                    // std::cout << "\n图像曝光异常，请调整摄像头位置\n" << std::endl;
                }
                // roi提取合格
                else {
                    // 将灰度roi转化成三通道灰度格式 准备用于推理模型输入
                    // _roi_img = add_channels(_roi_img);
                    cv::cvtColor(_roi_img, _roi_img, CV_GRAY2BGR);
                }
            }
        }
    }

    return _roi_fail;
}

// 用于roi成功提取到后的特征提取 返回特征向量
std::vector<float> CNet::getFeature() {
    // 将roi_img处理成特征提取模型输入需要的格式
    cv::dnn::blobFromImage(_roi_img, _blob_feature, 1 / 255.0, cv::Size(_roi_img_size, _roi_img_size), cv::Scalar(0, 0, 0), false, false);
    _feature_net.setInput(_blob_feature);

    // 前向预测得到输出
    // ========= 注意！！！========
    // 这里必须要用这种格式，因为这种格式是函数模板，支持_feature的类型，_feature_net.forward(_feature)这种格式不行！！
    _feature = _feature_net.forward();

    // 特征归一化
    // _feature = normalize(_feature);

    return _feature;
}

// 获取最新 roi 图像
const cv::Mat& CNet::getRoiImg() const {
    return _roi_img;
}

// 获取最新 roi 提取成功与否标志位
const bool& CNet::getRoiFail() const {
    return _roi_fail;
}

// 获取关键点坐标
const std::vector<cv::Point>& CNet::getPoints() const {
    return _points;
}

// 获取roi矩形的四个顶点
const std::vector<cv::Point>& CNet::getRect() const {
    return _roi_rect;
}