#include "CSession.h"
#include "LogicSystem.h"

// 初始化注册回调函数
// 启动逻辑线程
LogicSystem::LogicSystem() : _b_stop(false) {
    // 注册回调函数
    registerCallBacks();
    // 加载数据库
    _userdata_path = R"(../../res/data.dat)";
    _match_thres = 0.92;
    userdataLoad();
    _worker_thread = std::thread(&LogicSystem::dealTask, this);
}

// 执行完所有的任务，再释放线程  用于外部调用
LogicSystem::~LogicSystem() {
    {
        std::unique_lock<std::mutex> lock(_mtx);
        _b_stop = true;
        _consume.notify_one();
    }
    _worker_thread.join();
}

// 将回调函数和ID注册到map中
void LogicSystem::registerCallBacks() {
    _fun_callbacks[USERNAME_CHECK_ID] = std::bind(&LogicSystem::usernameCheckCB, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    _fun_callbacks[FEATURE_MATCH_ID] = std::bind(&LogicSystem::featureMatchCB, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    _fun_callbacks[DATABASE_UPDATE_ID] = std::bind(&LogicSystem::databaseUpdateCB, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
}

// 工作线程启动函数
void LogicSystem::dealTask() {
    for (;;) {
        std::unique_lock<std::mutex> lock(_mtx);
        _consume.wait(lock, [this]() {
            return _b_stop || !_task_que.empty();
        });
        // 如果收到暂停信号，则执行完所有任务退出循环
        if (_b_stop) {
            if (_task_que.empty()) break;
            while (_task_que.size() > 0) {
                auto task = _task_que.front();
                auto it = _fun_callbacks.find(task->_recv_node->_msg_id);
                if (it == _fun_callbacks.end()) {
                    _task_que.pop();
                    continue;
                }
                it->second(task->_session, task->_recv_node->_msg_id,
                    std::string(task->_recv_node->_data, task->_recv_node->_total_len));
                _task_que.pop();
            }
            break;
        }
        // 服务器正常运转
        auto task = _task_que.front();
        auto it = _fun_callbacks.find(task->_recv_node->_msg_id);
        if (it == _fun_callbacks.end()) {
            _task_que.pop();
            lock.unlock();
            continue;
        }
        _task_que.pop();
        lock.unlock();
        it->second(task->_session, task->_recv_node->_msg_id,
            std::string(task->_recv_node->_data, task->_recv_node->_total_len));
    }
}

// 加载数据库
void LogicSystem::userdataLoad() {
    std::ifstream fin;

    // 二进制读取
    fin.open(_userdata_path, std::ios::in | std::ios::binary);
    if (!fin.is_open()) {
        // 文件不存在的情况
        if (fin.fail() && !fin.bad()) {
            // std::cout << "\n数据文件不存在 已创建\n";
            std::ofstream createFile(_userdata_path);
        }
        // 文件存在但打不开
        else {
            std::cout << "\n数据文件无法打开 请检查\n";
            exit(-1);
        }
    }
    // 先判断文件是否为空
    if (fin.peek() == std::ifstream::traits_type::eof()) {
        // std::cout << "\n数据库为空\n";
        return;
    }

    // 读取到内存，储存在 m_user_database 中
    while (true) {
        UserData user;
        int name_length, feature_size;

        fin.read(reinterpret_cast<char*>(&name_length), sizeof(name_length));

        // 注意位置 EOF在执行read函数后才改变，这行代码必须放这里
        if (fin.eof()) break;

        user.name.resize(name_length);
        fin.read(&user.name[0], name_length);

        fin.read(reinterpret_cast<char*>(&feature_size), sizeof(feature_size));
        user.feature.resize(feature_size);
        fin.read(reinterpret_cast<char*>(user.feature.data()), sizeof(float) * feature_size);

        _user_database.push_back(user);
    }

    fin.close();
    // std::cout << "\n数据库加载完成\n";
}


// 用户名检查
// 有重复则为false
bool LogicSystem::usernameCheck(std::string& username) {
    auto it = std::find_if(_user_database.begin(), _user_database.end(), [&username](const UserData& data){
        return data.name == username;
    });
    if (it != _user_database.end()) return false;
    return true;
}

// 用户名检查回调函数
/*
接收消息结构：
{
    "data" : {
        "username" : string
    }
}
发送消息结构：
{
    "data" : {
        "result" : bool
    }    
}
*/
void LogicSystem::usernameCheckCB(std::shared_ptr<CSession> session, short msg_id, std::string msg_data) {
    // 解析收到的json消息体
    Json::Value recv;
    Json::Reader reader;
    reader.parse(msg_data, recv);
    std::string username = recv["data"]["username"].asString();

    // 用户名检查
    bool result = usernameCheck(username);

    // 构建发送的消息体
    Json::Value send;
    send["data"]["result"] = result;
    std::string send_msg = send.toStyledString();
    // XXX：不知道能不能这样用
    session->send(send_msg, msg_id);
}


// 特征向量匹配
int LogicSystem::featureMatch(std::vector<float>& feature, float& threshold) {
    // 数据库为空的情况
    if (_user_database.size() == 0) return -1;

    // 将vector转化成eigen向量
    Eigen::Map<Eigen::VectorXf> e_feature(feature.data(), LEN_FEATURE);

    // 余弦相似度
    int index = -1;
    std::vector<float> v_result;
    for (auto it = _user_database.begin(); it != _user_database.end(); ++it)
    {
        Eigen::Map<Eigen::VectorXf> temp_feature(it->feature.data(), LEN_FEATURE);
        float result = static_cast<float>(e_feature.dot(temp_feature) / (e_feature.norm() * temp_feature.norm()));
        v_result.push_back(result);
    }

    // 查找最大元素所在索引
    std::vector<float>::iterator max_it = std::max_element(v_result.begin(), v_result.end());
    if (max_it != v_result.end())
    {
        if (*max_it < _match_thres) index = -1;
        index = std::distance(v_result.begin(), max_it);
    }
    return index;   // 缺省返回-1，表示查找失败
}

/*
接收消息结构：
    {
        "data" : {
            "feature" : [0.1, ...]
        }
    }
发送消息结构：
    {
        "data" : {
            "result" : bool,
            "username" : string
        }
    }
*/
// 特征向量匹配回调函数
void LogicSystem::featureMatchCB(std::shared_ptr<CSession> session, short msg_id, std::string msg_data) {
    // 解析收到的json消息体
    Json::Value recv;
    Json::Reader reader;
    reader.parse(msg_data, recv);
    std::vector<float> feature;
    const Json::Value& floatArrayJson = recv["data"]["feature"];
    for (const auto& floatValue : floatArrayJson) {
        feature.emplace_back(floatValue.asFloat());
    }

    // 特征向量匹配
    int index = featureMatch(feature, _match_thres);
    bool result;
    std::string username;
    if (index != -1) {
        result = true;
        username = _user_database[index].name;
    } else {
        result = false;
        username = R"()";
    }
    
    // 构建发送消息体
    Json::Value send;
    send["data"]["result"] = result;
    send["data"]["username"] = username;
    std::string send_msg = send.toStyledString();
    // XXX：不知道能不能这样用
    session->send(send_msg, msg_id);
}

// 数据库更新
bool LogicSystem::databaseUpdate(std::string& name, std::vector<float>& feature) {
    _user_database.emplace_back(std::move(UserData{name, feature}));

    std::ofstream fout;

    // 设置清空原文件数据并以二进制写入
    fout.open(_userdata_path, std::ios::out | std::ios::binary);
    if (!fout.is_open()) {
        std::cout << "\n数据保存失败\n";
        return false;
    }

    for (const UserData& data : _user_database) {
        // 先写入用户名长度和C风格用户名
        int name_length = data.name.length();
        fout.write(reinterpret_cast<const char*>(&name_length), sizeof(name_length));
        fout.write(data.name.c_str(), name_length);

        // 写入特征向量大小和特征向量
        int feature_size = data.feature.size();
        fout.write(reinterpret_cast<const char*>(&feature_size), sizeof(feature_size));
        fout.write(reinterpret_cast<const char*>(data.feature.data()), sizeof(float) * feature_size);
    }

    fout.close();
    return true; 
}

/*
接收消息结构：
    {
        "data" : {
            "username" : string,
            "feature" : [0.1, ...]
        }
    }
发送消息结构：
    {
        "data" : {
            "result" : bool
        }
    }
*/
// 数据库更新回调函数
void LogicSystem::databaseUpdateCB(std::shared_ptr<CSession> session, short msg_id, std::string msg_data) {
    // 解析接收到的json消息体
    Json::Value recv;
    Json::Reader reader;
    reader.parse(msg_data, recv);
    std::string username = recv["data"]["username"].asString();
    std::vector<float> feature;
    const Json::Value& floatArrayJson = recv["data"]["feature"];
    for (const auto& floatValue : floatArrayJson) {
        feature.emplace_back(floatValue.asFloat());
    }

    // 更新数据库
    bool result = databaseUpdate(username, feature);

    // 构建发送消息体
    Json::Value send;
    send["data"]["result"] = result;
    std::string send_msg = send.toStyledString();
    // XXX：不知道能不能这样用
    session->send(send_msg, msg_id);
}

// 共有：将任务放入逻辑任务队列
void LogicSystem::postMsgToQue(std::shared_ptr<LogicNode> msg) {
    std::unique_lock<std::mutex> lock(_mtx);
    _task_que.emplace(msg);
    _consume.notify_one();
}