#ifndef _LOGICSYSTEM_H
#define _LOGICSYSTEM_H

#include <iostream>
#include <queue>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <string>
#include <functional>
#include <vector>
#include <fstream>

#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <Eigen/Dense>

#include "Singleton.hpp"
#include "ConstVar.h"

class LogicNode;
class CSession;

struct UserData
{
public:
    std::string name;           // 用户名
    std::vector<float> feature; // 特征向量
};

typedef std::function<void(std::shared_ptr<CSession>, short msg_id, std::string msg_data)> funCallBack;
class LogicSystem : public Singleton<LogicSystem>
{
    friend class Singleton<LogicSystem>;
public:
    void postMsgToQue(std::shared_ptr<LogicNode> msg);
    ~LogicSystem();
private:
    LogicSystem();
    void registerCallBacks();
    void dealTask();

    void userdataLoad();

    bool usernameCheck(std::string& username);
    void usernameCheckCB(std::shared_ptr<CSession>, short msg_id, std::string msg_data);

    int featureMatch(std::vector<float>& feature, float& threshold);
    void featureMatchCB(std::shared_ptr<CSession>, short msg_id, std::string msg_data);

    bool databaseUpdate(std::string& name, std::vector<float>& feature);
    void databaseUpdateCB(std::shared_ptr<CSession>, short msg_id, std::string msg_data);
private:
    std::thread _worker_thread;                             // 逻辑线程
    std::queue<std::shared_ptr<LogicNode>> _task_que;       // 逻辑任务队列
    std::mutex _mtx;                                        // 互斥量
    std::condition_variable _consume;                       // 条件变量
    bool _b_stop;                                           // 逻辑线程停止信号
    std::map<short, funCallBack> _fun_callbacks;            // ID回调函数键值对

    std::vector<UserData> _user_database;                   // 数据库储存变量
    float _match_thres;                                     // 匹配阈值
    std::string _userdata_path;                             // 数据库储存地址
};

#endif