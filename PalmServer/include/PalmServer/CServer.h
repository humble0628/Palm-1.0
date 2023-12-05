#ifndef _CSERVER_H
#define _CSERVER_H

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <boost/asio.hpp>

class CSession;

class CServer
{
public:
	CServer(boost::asio::io_context& ioc, unsigned short port);
	void clearSession(std::string& uuid);
private:
	void startAccept();
	void handleAccept(std::shared_ptr<CSession> new_session, const boost::system::error_code& error);

private:
	boost::asio::io_context& _ioc;
	boost::asio::ip::tcp::acceptor _acceptor;

	std::map<std::string, std::shared_ptr<CSession>> _sessions;
};

#endif