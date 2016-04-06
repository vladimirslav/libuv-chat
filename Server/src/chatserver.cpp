#include "chatserver.h"
#include <stdio.h>
#include <iostream>
#include <cstring>
#include <algorithm>

static const int DEFAULT_BACKLOG = 100;
static const int DISCONNECTION_TIME = 10000;

void Log(std::string str)
{
    std::cout << str << std::endl;
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    
    ChatServer* server = ChatServer::GetInstance();
    msg_buffer* readVector = server->GetReadBuffer();
    *buf = uv_buf_init(readVector->data(), readVector->capacity());  
}

void ChatServer::OnMsgRecv(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    Log("Msg received!");
    auto connection_pos = open_sessions.find(stream);
    if (connection_pos != open_sessions.end())
    {
        bool reset_timer = true;
        if (nread == UV_EOF)
        {
            reset_timer = false;
            RemoveClient(stream, connection_pos->second.IsActive(), ChatServer::DisconnectionReason::ConnectionClosed);
            Log("Client Disconnected");
        }
        else if (nread > 0)
        {
            const char* charbuffer = buf->base;
            int n = nread;

            while (n > 0)
            {
                const char* zero = (char*)memchr(charbuffer, 0, n);
                if (zero == nullptr)
                {
                    connection_pos->second.AddToMsg(charbuffer, n);
                    n = 0;
                }
                else
                {
                    ChatSession* s = &connection_pos->second;
                    int bufflen = zero - charbuffer;
                    s->AddToMsg(charbuffer, bufflen);
                    if (s->GetReadState() == ChatSession::ReadState::NameRead)
                    {
                        s->FinishMessage();
                        // Time to check for name!
                        std::string new_name = s->GetName();
                        auto name_pos = std::find(name_list.begin(), name_list.end(), new_name);
                        if (name_pos != name_list.end())
                        {
                            // name already exists
                            SendSingleMsg(stream, "Chosen name already exists!");
                            RemoveClient(stream, false, ChatServer::DisconnectionReason::DuplicateName);
                            n = 0;
                        }
                        else
                        {
                            s->Activate();
                            name_list.push_back(s->GetName());
                            Broadcast(s->GetName() + " has joined!"); 
                        }
                    }
                    else
                    {
                        Broadcast(s->GetName() + ":" + s->GetMsg());
                        s->FinishMessage();
                    }
                    charbuffer = zero + 1;
                    n -= bufflen + 1;
                }
            } 
        }
        else if (nread < 0)
        {
            Log("Error: " + std::to_string(nread));
        }
        else if (nread == 0)
        {
            Log("Zero msg size received");
            // nothing bad is happening, but consider logging it
        }

        if (reset_timer)
        {
            uv_timer_start(connection_pos->second.activity_timer.get(), 
                           [] (uv_timer_t* handle)
                           {
                               ChatServer::GetInstance()->OnClientTimeout(handle);
                           },
                           DISCONNECTION_TIME,
                           0);
               
        }
    }
    else
    {
        uv_read_stop(stream);
        Log("Unrecognized client. Disconnecting.");
    }
}

void ChatServer::RemoveClient(uv_stream_t* client, bool remove_name_from_list, DisconnectionReason reason)
{
    auto connection_pos = open_sessions.find(client);
    if (connection_pos != open_sessions.end())
    {   
        std::string reasonstr = "Connection Closed";
        if (reason == DisconnectionReason::Timeout)
        {
            reasonstr = "Timeout";
        }
        else if (reason == DisconnectionReason::DuplicateName)
        {
            reasonstr = "Name already taken";
        }
        else if (reason == DisconnectionReason::Error)
        {
            reasonstr = "Server Error";
        }
        
        std::string name;
        if (remove_name_from_list)
        {
            name = connection_pos->second.GetName();
            Log("Removing name '" + name + "' from list");
            name_list.erase(std::remove(name_list.begin(), name_list.end(), name), name_list.end());
        }


        if (reason != DisconnectionReason::ConnectionClosed)
        {
            SendSingleMsg(client, "You have been disconnected(" + reasonstr + ")");
        }
      
        uv_timer_stop(connection_pos->second.activity_timer.get());
        active_timers.erase(connection_pos->second.activity_timer.get());
        uv_read_stop(client);
        
        connection_pos->second.Deactivate();

        if (remove_name_from_list)
        {
            Broadcast(name + " has left the chat(" + reasonstr + ")");
        }

        uv_close((uv_handle_t*)connection_pos->second.connection.get(), 
                 [] (uv_handle_t* handle)
                 {
                     ChatServer::GetInstance()->OnConnectionClose(handle);
                 });
    }
}

void ChatServer::OnConnectionClose(uv_handle_t* handle)
{
    open_sessions.erase((uv_stream_t*)handle);
}

void ChatServer::Broadcast(const std::string& msg)
{
    Log("Broadcasting message: " + msg + std::to_string(msg.size()));    
    if (name_list.size() > 0)
    {
        std::shared_ptr<Msg> msgStruct = std::make_shared<Msg>(msg);
        for (auto& session : open_sessions)
        {
            if (session.second.IsActive())
            {
                msg_queue.messages.push(msgStruct);
                SendData((uv_stream_t*)session.second.connection.get(), msg_queue.messages.back().get());
            }
        }  
    }
    else
    {
        //Not sending message to anyone as noone is fully connected yet
    }
}

void ChatServer::SendSingleMsg(uv_stream_t* target, std::string message)
{
    std::shared_ptr<Msg> new_msg = std::make_shared<Msg>(Msg(message));
    msg_queue.messages.push(new_msg);
    Log("Sending message: " + message +"[" + std::to_string(message.size()) + "]");
   
    SendData(target, msg_queue.messages.back().get());
}

void ChatServer::SendData(uv_stream_t* connection, Msg* message)
{
    uv_write_t* req = msg_queue.requests.GetNew();
    uv_write(req,
             connection,
             message->GetBuf(),
             1,
             [] (uv_write_t* req, int status)
             {
                 ChatServer::GetInstance()->OnMsgSent(req, status);
             });
}

void ChatServer::OnClientTimeout(uv_timer_t* handle)
{
    auto pos = active_timers.find(handle);
    if (pos != active_timers.end())
    {
        auto session_pos = open_sessions.find(pos->second);
        if (session_pos != open_sessions.end())
        {
            ChatSession* s = &(session_pos->second);
            RemoveClient((uv_stream_t*)s->connection.get(),
                         s->IsActive(),
                         ChatServer::DisconnectionReason::Timeout);
        }
    }
    else
    {
        Log("Session timeout timer activated, but client already left");
    }
}

void ChatServer::OnNewConnection(uv_stream_t* server, int status)
{
    Log("New Connection Attempt");
    if (status == 0)
    {
        ChatSession newSession; 
        newSession.connection = std::make_shared<uv_tcp_t>();
        newSession.activity_timer = std::make_shared<uv_timer_t>();
         
        uv_tcp_init(&loop, newSession.connection.get());
        uv_timer_init(&loop, newSession.activity_timer.get());
       
        Log("Trying to accept connection");
        if (uv_accept(server, (uv_stream_t*)newSession.connection.get()) == 0)
        {
            Log("Connection accepted!");
            uv_stream_t* key = (uv_stream_t*)newSession.connection.get();
            uv_read_start((uv_stream_t*)newSession.connection.get(),
                          alloc_buffer, 
                          [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
            {
                ChatServer::GetInstance()->OnMsgRecv(stream, nread, buf);
            });

            open_sessions.insert({key, 
                                newSession});
            active_timers.insert({newSession.activity_timer.get(), key});
            
            uv_timer_start(newSession.activity_timer.get(), 
                           [] (uv_timer_t* handle)
                           {
                               ChatServer::GetInstance()->OnClientTimeout(handle);
                           },
                           DISCONNECTION_TIME,
                           0);
           

            Log("Session saved!");
        }
        else
        {
            Log("Error accepting connection" + std::string(uv_strerror(status)));
            uv_read_stop((uv_stream_t*)newSession.connection.get());
        }
    }
    else
    {
       Log("Connection error: " +  std::string(uv_strerror(status)));
    }

}

ChatServer* ChatServer::GetInstance()
{
    static ChatServer server;
    return &server;
}

const size_t MAX_BUFF_SIZE = 4096;
ChatServer::ChatServer()
    : running(false)
{
    read_buffer.resize(MAX_BUFF_SIZE);
}

int ChatServer::Init(int port)
{
    uv_loop_init(&loop);
    uv_tcp_init(&loop, &server);

    sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", port, &addr);

    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

    int err = uv_listen((uv_stream_t*) &server, 
                        DEFAULT_BACKLOG, 
                        [](uv_stream_t* server, int status)
                        {
                            ChatServer::GetInstance()->OnNewConnection(server, status);
                        });

    if (err == 0)
    {
        std::cout << "Listening for connections on port " << port << std::endl;
        uv_run(&loop, UV_RUN_DEFAULT);
        running = true;
    }

    return err;   
}

msg_buffer* ChatServer::GetReadBuffer()
{
    return &read_buffer;
}

ChatServer::~ChatServer()
{
    uv_loop_close(&loop);
    std::cout << "Server Terminated";
}

ChatSession::ChatSession()
    : state(ReadState::NameRead)
    , active(false)
{
}

void ChatSession::AddToMsg(const char* msg_part, size_t size)
{
    next_msg.append(msg_part, size);
}

ChatSession::~ChatSession()
{
}

std::string ChatSession::GetName() const
{
    return name;
}

std::string ChatSession::GetMsg() const
{
    return next_msg;
}

void ChatSession::FinishMessage()
{
    if (state == ReadState::NameRead)
    {
        name = std::string(next_msg.begin(), next_msg.end());
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        state = ReadState::MessageRead;
    }
    next_msg.clear();
}

void ChatServer::OnMsgSent(uv_write_t* req, int status)
{
    if (msg_queue.messages.empty() == false)
    {
        msg_queue.messages.pop();
        msg_queue.requests.Release();
    }


    if (status != 0)
    {
        Log("Error sending message! " + std::string(uv_strerror(status)));
    }
}

ChatSession::ReadState ChatSession::GetReadState() const
{
    return state;
}

void ChatSession::Activate()
{
    active = true;
}

void ChatSession::Deactivate()
{
    active = false;
}

bool ChatSession::IsActive() const
{
    return active;
}

