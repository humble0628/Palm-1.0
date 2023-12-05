#ifndef _CLIENT_H
#define _CLIENT_h

#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

#include "ConstVar.h"

class RecvNode;
class SendNode;

class Client
{
public:
    Client() = delete;
    Client(boost::asio::io_context& ioc, std::string ip, short port);
    ~Client();
    void connect();
    Json::Value dealSession(short id, std::string& send_msg);

private:
    boost::asio::ip::tcp::socket _socket;
    boost::asio::ip::tcp::endpoint _remote_ep;
    boost::system::error_code _error;

    std::shared_ptr<SendNode> _send_node;
    char* _recv_data;
};

#endif