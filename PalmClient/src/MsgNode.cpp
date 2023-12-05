#include "MsgNode.h"

MsgNode::MsgNode(short total_len) : _total_len(total_len), _cur_len(0) {
    _data = new char[total_len + 1];
    _data[_total_len] = '\0';
}

void MsgNode::clear() {
    ::memset(_data, 0, _total_len);
    _cur_len = 0;
}

MsgNode::~MsgNode() {
    delete[] _data;
}

RecvNode::RecvNode(short total_len, short msg_id) : MsgNode(total_len), _msg_id(msg_id) {

}

RecvNode::~RecvNode() {}

SendNode::SendNode(const char* msg, short total_len, short msg_id) : MsgNode(total_len + HEAD_TOTAL_LEN), 
_msg_id(msg_id) {
    short msg_id_net = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
    memcpy(_data, &msg_id_net, HEAD_ID_LEN);
    short msg_len_net = boost::asio::detail::socket_ops::host_to_network_short(total_len);
    memcpy(_data + HEAD_ID_LEN, &msg_len_net, HEAD_MSG_LEN);
    memcpy(_data + HEAD_TOTAL_LEN, msg, total_len);
}

SendNode::~SendNode() {}