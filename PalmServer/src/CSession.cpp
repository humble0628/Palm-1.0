#include "CServer.h"
#include "CSession.h"
#include "LogicSystem.h"

// 会话初始化，得到对应的uuid
CSession::CSession(boost::asio::io_context& ioc, CServer* server) :  
_socket(ioc), _server(server), _b_head_parse(false), _b_close(false) {
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    _uuid = boost::uuids::to_string(a_uuid);
    _recv_head_node = std::make_shared<MsgNode>(HEAD_TOTAL_LEN);
}

CSession::~CSession() {
    std::cout << "\n会话: [" << _socket.remote_endpoint().address().to_string()<< "] 关闭\n" << std::endl;
    _socket.close();
}

// 启动会话，会话会不断将读取或发送添加到事件循环队列
// 通信为全双工
void CSession::start() {
    std::cout << "\n客户端: [" << _socket.remote_endpoint().address().to_string() << "] 建立连接\n";
    memset(_data, 0, MAX_LENGTH);
    boost::asio::async_read(_socket, boost::asio::buffer(_data, HEAD_TOTAL_LEN),
        std::bind(&CSession::handleRead, this, std::placeholders::_1, std::placeholders::_2, getShared()));
}

// 关闭会话
void CSession::close() {
	_b_close = true;
    _server->clearSession(_uuid);    
}

// 向客户端发送数据  c风格字符串
void CSession::send(const char* msg, short max_length, short msg_id) {
    int send_que_size = _send_que.size();
    // 将发送任务添加到发送队列
    _send_que.emplace(std::make_shared<SendNode>(msg, max_length, msg_id));

    // 如果任务队列中还有数据，则返回
    if (send_que_size > 0) {
		return;
	}

    // 如果队列中数据没有数据，则发送此次添加的数据
    auto& msgnode = _send_que.front();
    boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
        std::bind(&CSession::handleWrite, this, std::placeholders::_1, getShared()));
}

// 向客户端发送数据  c++ string
void CSession::send(const std::string msg, short msg_id) {
    int send_que_size = _send_que.size();
    // 将发送任务添加到发送队列
    _send_que.emplace(std::make_shared<SendNode>(msg.c_str(), msg.length(), msg_id));

    // 如果任务队列中还有数据，则返回
    if (send_que_size > 0) {
		return;
	}

    // 如果队列中数据没有数据，则发送此次添加的数据
    auto& msgnode = _send_que.front();
    boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
        std::bind(&CSession::handleWrite, this, std::placeholders::_1, getShared()));
}

// get私有成员 _socket
boost::asio::ip::tcp::socket& CSession::Socket() {
    return _socket;
}

// get私有成员 _uuid
std::string& CSession::getUuid() {
    return _uuid;
}

// get shared_from_this
std::shared_ptr<CSession> CSession::getShared() {
    return shared_from_this();
}

// 判断 ID 是否被包含在枚举变量中 被包含则返回true
bool CSession::isValueInEnum(MSG_IDS val) {
    switch (val) {
        case MSG_IDS::USERNAME_CHECK_ID:
        case MSG_IDS::FEATURE_MATCH_ID:
        case MSG_IDS::DATABASE_UPDATE_ID:
            return true;
        default:
            return false;
    }
}

// 读回调函数
void CSession::handleRead(const boost::system::error_code& error, std::size_t bytes_transferred, std::shared_ptr<CSession> _self_shared) {
    if (!error) {
        // 头部没有处理完
        if (!_b_head_parse) {

            // 如果头部长度和读取到的长度不一致，则直接关闭会话
            // TODO：这里能不能加一个不一致回执操作
            if (bytes_transferred != HEAD_TOTAL_LEN) close();

            // 将数据按字节拷贝到处理头部数据的节点
            memcpy(_recv_head_node->_data, _data, HEAD_TOTAL_LEN);

            // 将接收变量置为0
            ::memset(_data, 0, MAX_LENGTH);
            
            // 取出id
            short msg_id = 0;
            memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
            msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
            // id检查
            if (!isValueInEnum(static_cast<MSG_IDS>(msg_id))) close();

            // 取出消息体长度
            short msg_len = 0;
            memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_MSG_LEN);
            msg_len =  boost::asio::detail::socket_ops::network_to_host_short(msg_len);
            // 消息长度检查
            if (msg_len > MAX_LENGTH) close();

            // 打印请求日志
            std::cout << "\n客户端: [" << _socket.remote_endpoint().address().to_string() << "] 请求id: " << msg_id << "\n";

            // 多态使用，基类指针指向派生类对象
            // 此时构建头部数据，用于下一次处理消息体数据
            _recv_msg_node = std::make_shared<RecvNode>(msg_len, msg_id);
            boost::asio::async_read(_socket, boost::asio::buffer(_data, msg_len),
                std::bind(&CSession::handleRead, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
            _b_head_parse = true;
            return;
        }
        // 头部数据已经处理完
        // 数据长度不符，则关闭会话
        if (bytes_transferred != _recv_msg_node->_total_len) close();

        // 将消息构建成标准模式加入逻辑队列
        memcpy(_recv_msg_node->_data, _data, _recv_msg_node->_total_len);
        LogicSystem::getInstance()->postMsgToQue(std::make_shared<LogicNode>(_recv_msg_node, _self_shared));

        ::memset(_data, 0, MAX_LENGTH);
        boost::asio::async_read(_socket, boost::asio::buffer(_data, HEAD_TOTAL_LEN),
            std::bind(&CSession::handleRead, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
        _b_head_parse = false;
    }
	else {
		std::cout << "\nread error" << std::endl;
		close();
	}
}

// 写回调函数
void CSession::handleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> _self_shared) {
    if (!error) {
        // 执行到这说明消息发送成功，将队列中的第一个消息pop掉
        _send_que.pop();
        if (_send_que.size() > 0) {
            auto& msg = _send_que.front();
            boost::asio::async_write(_socket, boost::asio::buffer(msg->_data, msg->_total_len),
                std::bind(&CSession::handleWrite, this, std::placeholders::_1, _self_shared));
        }
    }
    else {
		std::cout << "\nwrite error" << std::endl;
		close();
    }
}

LogicNode::LogicNode(std::shared_ptr<RecvNode> recv_node, std::shared_ptr<CSession> session) : 
    _recv_node(recv_node), _session(session) {

}