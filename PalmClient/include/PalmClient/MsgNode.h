#ifndef _MSGNODE_H
#define _MSGNODE_H

#include <iostream>

#include <boost/asio.hpp>

#include "ConstVar.h"

class MsgNode
{
    friend class Client;
public:
    MsgNode(short total_len);
    ~MsgNode();
    void clear();
protected:
    short _cur_len;
    short _total_len;
    char* _data;
};

class RecvNode : public MsgNode
{
public:
    RecvNode(short total_len, short msg_id);
    ~RecvNode();
private:
    short _msg_id;
};

class SendNode : public MsgNode
{
public:
    SendNode(const char* msg, short total_len, short msg_id);
    ~SendNode();
private:
    short _msg_id;
};

#endif