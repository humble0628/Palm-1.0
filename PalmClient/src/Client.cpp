#include "Client.h"
#include "MsgNode.h"

Client::Client(boost::asio::io_context& ioc, std::string ip, short port) : 
_socket(ioc), _remote_ep(boost::asio::ip::address::from_string(ip), port),
_error(boost::asio::error::host_not_found) {
    _recv_data = new char[MAX_LENGTH];
}

Client::~Client() {
    delete[] _recv_data;
}

// 连接服务器
void Client::connect() {
    _socket.connect(_remote_ep, _error);
    if (_error) {
        std::cout << "connect failed, code is " << _error.value() << " error msg is " << _error.message();
        exit(-1);
    }
    std::cout << "\n成功连接服务器\n";
}

// 发送并阻塞等待服务器回传
Json::Value Client::dealSession(short id, std::string& send_msg) {
    // 先构建成tlv消息格式
    _send_node = std::make_shared<SendNode>(send_msg.c_str(), send_msg.length(), id);
    // 同步发送
    boost::asio::write(_socket, boost::asio::buffer(_send_node->_data, _send_node->_total_len));

    // 接收消息头部
    boost::asio::read(_socket, boost::asio::buffer(_recv_data, HEAD_TOTAL_LEN));
    // 取出id并判断
    short msg_id = 0;
    memcpy(&msg_id, _recv_data, HEAD_ID_LEN);
    msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
    if (msg_id != id) exit(-1);

    // 取出消息长度
    short msg_len = 0;
    memcpy(&msg_len, _recv_data + HEAD_ID_LEN, HEAD_MSG_LEN);
    msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
    // 消息长度检查
    if (msg_len > MAX_LENGTH) exit(-1);

    ::memset(_recv_data, 0, MAX_LENGTH);
    // 继续从缓冲区读取消息体
    boost::asio::read(_socket, boost::asio::buffer(_recv_data, msg_len));
    Json::Value result;
    Json::Reader reader;
    std::string recv_data(_recv_data, msg_len);
    reader.parse(recv_data, result);
    ::memset(_recv_data, 0, MAX_LENGTH);

    return result;
}