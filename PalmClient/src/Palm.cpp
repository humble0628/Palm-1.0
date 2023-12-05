#include "Palm.h"
#include "Client.h"

CPalm::CPalm(boost::asio::io_context& ioc) : _client(new Client(ioc, "10.182.20.183", 10086)) {
    // 检查License

    // 摄像头初始化
    _ir_frame = _camera.getFrame("ir", "GetFrameOlny").clone();

    // 模型初始化
    _roi_fail = _cnet.getRoiFail();
    _points = _cnet.getPoints();
    _roi_img = _cnet.getRoiImg();

    // 连接服务器
    _client->connect();

    // user变量初始化
    _user.name = "";
    _user.feature.resize(128, 0.0);

    _palm_fail = true;
    _img_scalar_fail = true;
}

CPalm::~CPalm() {

}


// 不能与数据库存在同名注册 通过与服务器通信反馈查询结果 有重复则为false
/*
接收消息结构：
{
    "data" : {
        "result" : bool
    }    
}
发送消息结构：
{
    "data" : {
        "username" : string
    }
}
*/
bool CPalm::usernameCheckCB(std::string& username) {
    Json::Value send, recv;
    send["data"]["username"] = username;
    std::string send_msg = send.toStyledString();
    recv = _client->dealSession(USERNAME_CHECK_ID, send_msg);

    return recv["data"]["result"].asBool();
}

// 返回特征向量的匹配结果
/*
接收消息结构：
    {
        "data" : {
            "result" : bool,
            "username" : string
        }
    }
发送消息结构：
    {
        "data" : {
            "feature" : [0.1, ...]
        }
    }
*/
Json::Value CPalm::featureMatchCB(std::vector<float>& feature) {
    Json::Value send, recv;
    for (int i = 0; i < LEN_FEATURE; ++i) {
        send["data"]["feature"].append(feature[i]);
    }
    std::string send_msg = send.toStyledString();
    recv = _client->dealSession(FEATURE_MATCH_ID, send_msg);

    return recv; 
}

// 将新注册的用户发送给服务器
/*
接收消息结构：
    {
        "data" : {
            "result" : bool
        }
    }
发送消息结构：
    {
        "data" : {
            "username" : string,
            "feature" : [0.1, ...]
        }
    }
*/
bool CPalm::databaseUpdateCB(std::string& username, std::vector<float>& feature) {
    Json::Value send, recv;
    send["data"]["username"] = username;
    for (int i = 0; i < LEN_FEATURE; ++i) {
        send["data"]["feature"].append(feature[i]);
    }
    std::string send_msg = send.toStyledString();
    recv = _client->dealSession(DATABASE_UPDATE_ID, send_msg);

    return recv["data"]["result"].asBool(); 
}

// 检查输入的用户名是否合法
bool CPalm::usernameCheck(std::string& username) {
    // 不能为纯数字
    if (std::all_of(username.begin(), username.end(), [](char c){
        return std::isdigit(c);
    })) {
        std::cout << "\n用户名不能为纯数字" << std::endl;
        return false;
    }
    
    // 不能过长或过短 
    if (username.size() > 12 || username.size() < 2) {
        std::cout << "\n用户名长度不符合要求" << std::endl;
        return false;
    }

    // 不能包含除下划线以外的特殊符号
    if (std::any_of(username.begin(), username.end(), [](char c){
        return !std::isalnum(c) && c != '_';
    })) {
        std::cout << "\n用户名不能包含除下划线以外的特殊符号" << std::endl;
        return false;
    }

    // 不能与数据库存在同名注册 通过与服务器通信反馈查询结果 有重复则为false
    if (!usernameCheckCB(username)) {
        std::cout << "\n用户名已存在" << std::endl;
        return false;
    }

    return true;
}

// 使一些 过程中一直存在但不断更改的变量 恢复缺省值
void CPalm::membersClear() {
    _roi_img.setTo(0);
    _roi_fail = true;
    _user.name = "";
    _user.feature.resize(128, 0.0);
    _features.clear();
    _palm_fail = true;
    _img_scalar_fail = true;
}

//*************** 接口函数 ***************
// 注册用户
void CPalm::userRegister() {
    // 用户名操作
    std::cout << std::endl;
    std::cout << "注意：用户名长度2-12位字符\n"
              << "     字符包含字母、数字、特殊符号\n"
              << "     不能为纯数字\n"
              << "     特殊符号只能包含'_'" << std::endl;
    std::cout << std::endl;

    std::cout << "\n请输入用户名" << std::endl;
    std::cout << ">>";
    std::cin >> _user.name;
    while (!usernameCheck(_user.name)) {
        std::cout << "\n用户名不合法，请重新输入" << std::endl;
        std::cout << ">>";
        std::cin >> _user.name;
    }

    // roi检测
    int img_count = 0;      // 有效图像数量
    int palm_count = 0;     // 手掌检测次数
    int roi_count = 0;      // roi检测次数

    std::cout << "\n正在检测手掌..." << std::endl;

    // ********** 需要检测图像有效性 **********
    // 检测手掌进入 并等待相机曝光
    while (true) {
        // 手掌检测时间过长 退出识别模式
        if (palm_count >= PALM_COUNT) {
            std::cout << "\n未检测到手掌或曝光异常" << std::endl;
            return;
        }
        // 检测到手掌且曝光正常退出循环
        if (!_palm_fail && !_img_scalar_fail) break;
        _ir_frame = _camera.getFrame("ir", "GetFrameOlny");
        
        // 必须先检测到手掌 再计算曝光是否正常
        _palm_fail = _cnet.palmDetect(_ir_frame);

        // ***********  再计算曝光  ************
        // TODO：需要一个 动态测量曝光正常范围 的方法
        if (!_palm_fail) {
            // 休眠0.5秒，等待对焦和曝光
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            cv::Scalar gray_scalar = cv::mean(_ir_frame);
            if (gray_scalar.val[0] > 95 || gray_scalar.val[0] < 75) _img_scalar_fail = true;
            else _img_scalar_fail = false;
        }
        ++palm_count;
    }
    std::cout << "\n检测到手掌，请保持不要动" << std::endl;

    std::cout << "\n开始注册..." << std::endl;

    // roi提取
    while (true) {
        // ROI检测时间过长 退出识别模式
        if (roi_count >= ROI_COUNT) {
            std::cout << "\n没有采集到有效图像" << std::endl;
            membersClear();
            return;
        }
        // 检测到一定数量的有效roi 退出循环
        if (img_count >= IMG_NUM_RECOGNIZE) break;

        _ir_frame = _camera.getFrame("ir", "GetFrameOlny").clone();
        _roi_fail = _cnet.roiDetect(_ir_frame);
        if (!_roi_fail) {
            ++img_count;
            _roi_img = _cnet.getRoiImg();

            // 特征提取
            _features.push_back(_cnet.getFeature());
        }
        ++roi_count;
    }

    // 有效特征取平均
    for (const auto& feature : _features) {
        for (int i = 0; i < LEN_FEATURE; ++i) {
            _user.feature[i] += feature[i];
        }
    }
    for (int i = 0; i < LEN_FEATURE; ++i) {
        _user.feature[i] /= _features.size();
    }
    // *** 测试用 ***
    // for (auto it = _user.feature.begin(); it != _user.feature.end(); ++it)
    // {
    //     std::cout << *it << " ";
    // }
    // std::cout << std::endl;

    std::cout << "\n用户 " << _user.name << " 注册成功" << std::endl;

    // 更新数据库
    if (!databaseUpdateCB(_user.name, _user.feature)) {
        std::cout << "\n数据库更新失败" << std::endl;
        exit(-1);
    }
    std::cout << "\n数据库已更新" << std::endl;
    
    // 清空部分成员属性
    membersClear();
}

// 识别用户
void CPalm::userRecognize() {
    // roi检测
    int img_count = 0;      // 有效图像数量
    int palm_count = 0;     // 手掌检测次数
    int roi_count = 0;      // roi检测次数

    std::cout << "\n正在检测手掌..." << std::endl;

    // ********** 需要检测图像有效性 **********
    // 检测手掌进入 并等待相机曝光
    while (true) {
        // 手掌检测时间过长 退出识别模式
        if (palm_count >= PALM_COUNT) {
            std::cout << "\n未检测到手掌或曝光异常" << std::endl;
            return;
        }
        // 检测到手掌且曝光正常退出循环
        if (!_palm_fail && !_img_scalar_fail) break;
        _ir_frame = _camera.getFrame("ir", "GetFrameOlny");
        
        // 必须先检测到手掌 再计算曝光是否正常
        _palm_fail = _cnet.palmDetect(_ir_frame);

        // ***********  再计算曝光  ************
        // TODO：需要一个 动态测量曝光正常范围 的方法
        if (!_palm_fail) {
            // 休眠0.5秒，等待对焦和曝光
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            cv::Scalar gray_scalar = cv::mean(_ir_frame);
            if (gray_scalar.val[0] > 95 || gray_scalar.val[0] < 75) _img_scalar_fail = true;
            else _img_scalar_fail = false;
            // std::cout << gray_scalar.val[0] << std::endl;
        }
        ++palm_count;
    }
    std::cout << "\n检测到手掌，请保持不要动" << std::endl;

    // 开始计时
    auto start = std::chrono::high_resolution_clock::now();

    // roi提取
    while (true) {
        // ROI检测时间过长 退出识别模式
        if (roi_count >= ROI_COUNT) {
            std::cout << "\n没有采集到有效图像" << std::endl;
            membersClear();
            return;
        }
        // 检测到一定数量的有效roi 退出循环
        if (img_count >= IMG_NUM_RECOGNIZE) break;

        _ir_frame = _camera.getFrame("ir", "GetFrameOlny");
        _roi_fail = _cnet.roiDetect(_ir_frame);
        if (!_roi_fail) {
            ++img_count;
            _roi_img = _cnet.getRoiImg();

            // 特征提取
            _features.push_back(_cnet.getFeature());
        }
        ++roi_count;
    }

    // 有效特征取平均
    for (const auto& feature : _features) {
        for (int i = 0; i < LEN_FEATURE; ++i) {
            _user.feature[i] += feature[i];
        }
    }
    for (int i = 0; i < LEN_FEATURE; ++i) {
        _user.feature[i] /= _features.size();
    }

    // 特征匹配
    Json::Value root = featureMatchCB(_user.feature);
    if (!root["data"]["result"].asBool()) {
        std::cout << "\n未找到该用户" << std::endl;
        membersClear();
        return;        
    }
    std::string temp_name = root["data"]["username"].asString();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "\n识别成功：用户 " << temp_name << "  用时："<< duration.count() << " ms" << std::endl;

    // 清空部分成员属性
    membersClear();
}

// 数据库操作
void CPalm::userDatabase() {

}

// 退出程序
void CPalm::userQuit() {
    // 退出码为0 程序正常退出
    exit(0);
}

// 在每一帧图上画出关键点   用于子线程
const cv::Mat CPalm::getFrameWithPoints() {
    cv::Mat frame = _camera.getFrame("ir").clone();
    if (!_roi_fail) {
        _points = _cnet.getPoints();
        _roi_rect = _cnet.getRect();
        cv::circle(frame, _points[0], 3, cv::Scalar(0, 0, 255), 4);
        cv::circle(frame, _points[1], 3, cv::Scalar(255, 0, 0), 4);
        cv::circle(frame, _points[2], 3, cv::Scalar(255, 0, 0), 4);
        cv::polylines(frame, _roi_rect, true, cv::Scalar(0, 255, 0), 2);
        // *** 测试用 ***
        // cv::imwrite(R"(../../res/whole.jpg)", frame);
        // cv::imwrite(R"(../../res/roi.jpg)", _roi_img);
    }
    return frame;
}