#ifndef _CSESSION_H
#define _CSESSION_H

#include <iostream>
#include <memory>
#include <string>
#include <queue>

#include <boost/asio.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "ConstVar.h"
#include "MsgNode.h"

class CServer;
class LogicSystem;

class CSession : public std::enable_shared_from_this<CSession>
{
public:
	CSession() = delete;
	CSession(boost::asio::io_context& ioc, CServer* server);
	~CSession();

	void start();
	void close();
	void send(const char* msg, short max_length, short msg_id);
	void send(const std::string msg, short msg_id);
    
	boost::asio::ip::tcp::socket& Socket();
	std::string& getUuid();
    std::shared_ptr<CSession> getShared();
    bool isValueInEnum(MSG_IDS val);

private:
	void handleRead(const boost::system::error_code& error, std::size_t bytes_transferred, std::shared_ptr<CSession> _self_shared);
	void handleWrite(const boost::system::error_code& error, std::shared_ptr<CSession> _self_shared);

private:
	boost::asio::ip::tcp::socket _socket;
	char _data[MAX_LENGTH];
	CServer* _server;
	std::string _uuid;

	std::queue<std::shared_ptr<SendNode>> _send_que;

	std::shared_ptr<RecvNode> _recv_msg_node;	// 消息接收
	std::shared_ptr<MsgNode> _recv_head_node;	// 头部接收

	bool _b_head_parse;							// 头部处理完成标志位
	bool _b_close;								// 会话关闭标志位
};

class LogicNode
{
    friend class LogicSystem;
public:
    LogicNode(std::shared_ptr<RecvNode> recv_node, std::shared_ptr<CSession> session);
private:
    std::shared_ptr<RecvNode> _recv_node;
    std::shared_ptr<CSession> _session;
};

#endif