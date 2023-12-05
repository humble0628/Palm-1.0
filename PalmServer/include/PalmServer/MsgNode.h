#ifndef _MSGNODE_H
#define _MSGNODE_H

#include <iostream>

#include <boost/asio.hpp>

#include "ConstVar.h"

class CSession;

class MsgNode
{
    friend class CSession;
    friend class LogicSystem;
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
    friend class CSession;
    friend class LogicSystem;
public:
    RecvNode(short total_len, short msg_id);
    ~RecvNode();
private:
    short _msg_id;
};

class SendNode : public MsgNode
{
    friend class CSession;
    friend class LogicSystem;
public:
    SendNode(const char* msg, short total_len, short msg_id);
    ~SendNode();
private:
    short _msg_id;
};

#endif