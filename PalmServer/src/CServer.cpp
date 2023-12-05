#include "CServer.h"
#include "CSession.h"

// 初始化服务器端点
// 将accept加入 io_context 事件循环
CServer::CServer(boost::asio::io_context& ioc, unsigned short port) : _ioc(ioc),
_acceptor(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
    std::cout << "\n服务器启动，端口为: " << port << std::endl;
    startAccept();
}

// 从会话map中删除指定会话
void CServer::clearSession(std::string& uuid) {
	_sessions.erase(uuid);
}

// 用于将accept加入 io_context 事件循环
void CServer::startAccept() {
    std::shared_ptr<CSession> new_session = std::make_shared<CSession>(_ioc, this);
    _acceptor.async_accept(new_session->Socket(), std::bind(&CServer::handleAccept, this,
        new_session, std::placeholders::_1));
}

// acceptor回调函数，必须要有一个参数是 error_code
void CServer::handleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code& error) {
    // 如果捕捉到客户端连接
    if (!error) {
        new_session->start();
        _sessions.insert(std::make_pair(new_session->getUuid(), new_session));
    }
    // 如果没有捕捉到，则继续将acceptor加入事件队列
    startAccept();

    // 回调函数结束，没有捕捉到连接的会话生命周期将结束
}