#pragma once

#include "msg.h"

#include <uv.h>
#include <string>
#include <vector>
#include <queue>
#include <memory>

class ChatSession
{
public:
    static ChatSession* GetInstance();
    void Init(int port, const std::string& addr, const std::string& name);
    void ScheduleReconnect();

    void StdinRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    void OnConnect(uv_connect_t* connection, int status);
    void OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    void OnMsgSent(uv_write_t* req, int status);

    void SendMsg(Msg* msg);

    std::vector<char>* GetReadBuffer();
private:
    ChatSession();
    ~ChatSession();
    void Connect();
    uv_loop_t mainloop;
    uv_tcp_t socket;
    uv_connect_t connection;
    uv_stream_t* connection_handle;
    uv_timer_t reconnection_timer;
    uv_timer_t connection_timer;
    struct sockaddr_in dest;
    bool main_loop_running;
    bool sending_name;
    std::string name;

    std::string next_message;
    uv_pipe_t user_input;

    std::vector<char> read_buffer;
    MsgPool outgoing_queue;
    
};
