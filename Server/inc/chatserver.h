#pragma once

#include <uv.h>

#include "msg.h"

#include <map>
#include <queue>
#include <memory>

class ChatSession
{
public:
    enum class ReadState
    {
        NameRead,
        MessageRead
    };

    ChatSession();
    ~ChatSession();

    void SendMessage(Msg* message);
    void FinishMessage();

    void AddToMsg(const char* data, size_t len);

    std::string GetMsg() const;
    std::string GetName() const;
    ReadState GetReadState() const;

    std::shared_ptr<uv_tcp_t> connection;
    std::shared_ptr<uv_timer_t> activity_timer;

    void Activate();
    void Deactivate();
    bool IsActive() const;

protected:
    std::string name;
    std::string next_msg;

    ReadState state;
    bool active;
};

typedef std::map<uv_stream_t*, ChatSession> session_map_t;
typedef std::map<uv_timer_t*, uv_stream_t*> timer_map_t;

class ChatServer
{
public:
    enum class DisconnectionReason
    {
        Timeout,
        ConnectionClosed,
        DuplicateName,
        Error
    };

    static ChatServer* GetInstance();
    int Init(int port);

    void OnNewConnection(uv_stream_t* server, int status);
    void OnMsgRecv(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    void OnMsgSent(uv_write_t* req, int status);

    void Broadcast(const std::string& msg);
    void RemoveClient(uv_stream_t* client, bool remove_name_from_list, DisconnectionReason reason);
    void SendSingleMsg(uv_stream_t* target, std::string message);

    void SendData(uv_stream_t* connection, Msg* message);
    void OnConnectionClose(uv_handle_t* handle);

    void OnClientTimeout(uv_timer_t* handle);
    msg_buffer* GetReadBuffer();
protected:
    ChatServer();
    ~ChatServer();
 
    void AcceptConnections();

    uv_loop_t loop;
    uv_tcp_t server;
    bool running;

    session_map_t open_sessions;
    timer_map_t active_timers;
    std::vector<std::string> name_list;
    msg_buffer read_buffer;

    MsgPool msg_queue;
};


