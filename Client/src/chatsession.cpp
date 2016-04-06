#include "chatsession.h"

#include <iostream>
#include <functional>
#include <algorithm>

static const int CONNECTION_TIME = 3000; //3sec before we decide that we failed to connect
static const int RECONNECTION_TIME = 5000; //5 sec before we retry

static void display_line(std::string line)
{
    fprintf(stderr, "%s\n", line.c_str()); 
}

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    std::vector<char>* read_buffer = ChatSession::GetInstance()->GetReadBuffer();
    *buf = uv_buf_init(read_buffer->data(), read_buffer->capacity());    
}

void ChatSession::StdinRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    if (nread < 0)
    {
        display_line("Read stop");
        uv_read_stop(stream);
    }
    else
    {
        std::string msg(buf->base, nread - 1); // do not take newline
        std::shared_ptr<Msg> new_msg = std::make_shared<Msg>(Msg(msg));
        outgoing_queue.messages.push(new_msg);
        ChatSession::GetInstance()->SendMsg(outgoing_queue.messages.back().get());   
    }
}

void ChatSession::OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    if (nread == UV_EOF)
    {
        display_line("Disconnected");
        uv_read_stop(stream);
        uv_read_stop((uv_stream_t*)&user_input);
    }
    else if (nread > 0)
    {
        display_line(buf->base);
        next_message.append(buf->base);
    }
    else if (nread == 0)
    {
        next_message.clear();
    }
    else
    {
        display_line("Error reading data");
        uv_read_stop(stream);
        uv_read_stop((uv_stream_t*)&user_input);
    }
}

ChatSession* ChatSession::GetInstance()
{
    static ChatSession session;
    return &session;
}

void ChatSession::OnMsgSent(uv_write_t* req, int status)
{
    outgoing_queue.messages.pop();
    outgoing_queue.requests.Release();

    if (status == 0)
    {
        if (sending_name)
        {
            sending_name = false;
            // name approved, now we can start making normal input
            const int STDIN_DESCRIPTOR = 0;
            uv_pipe_init(&mainloop, &user_input, false);
            uv_pipe_open(&user_input, STDIN_DESCRIPTOR);

            uv_read_start((uv_stream_t*)&user_input,
                          alloc_buffer,
                          [] (uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
                          {
                              ChatSession::GetInstance()->StdinRead(stream, nread, buf);
                          });
            }

    }
    else
    {
        display_line("Error writing message");
    }
}

void ChatSession::SendMsg(Msg* message)
{
    // we know that send is scheduled right after input
    // so no point to make multiple 'req'
    uv_write(outgoing_queue.requests.GetNew(),
             connection_handle,
             message->GetBuf(),
             1,
             [] (uv_write_t* req, int status)
             {
                 ChatSession::GetInstance()->OnMsgSent(req, status);
             });
}

void ChatSession::OnConnect(uv_connect_t* connection, int status)
{
    uv_timer_stop(&connection_timer);
    if (status == 0)
    {
        display_line("Connected!");
        uv_read_start(connection->handle,
                      alloc_buffer, 
                      [] (uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
                      {
                          ChatSession::GetInstance()->OnRead(stream, nread, buf);
                      });

        connection_handle = connection->handle;
       
        // now send the name
        std::shared_ptr<Msg> new_msg = std::make_shared<Msg>(Msg(name));
        outgoing_queue.messages.push(new_msg);
        ChatSession::GetInstance()->SendMsg(outgoing_queue.messages.back().get());
    }
    else
    {
        display_line("Connection unsuccessful:");
        display_line(uv_strerror(status));
        
        ScheduleReconnect();
    }
   
}

void ChatSession::ScheduleReconnect()
{
    display_line("Connection failed");
    uv_timer_stop(&connection_timer);
    display_line("Trying to reconnect after 5 seconds");
    uv_timer_start(&reconnection_timer,
                   [] (uv_timer_t* handle)
                   {
                       ChatSession* session = ChatSession::GetInstance();
                       session->Connect();
                   }, RECONNECTION_TIME, 0);
}

void ChatSession::Connect()
{
    display_line("Connecting...");
    uv_tcp_connect(&connection, 
                   &socket, 
                   (const struct sockaddr*)&dest, 
                   [] (uv_connect_t* connection, int status)
                   {
                       ChatSession::GetInstance()->OnConnect(connection, status);
                   });
    uv_timer_start(&connection_timer,
                   [] (uv_timer_t* handle)
                   {
                       ChatSession::GetInstance()->ScheduleReconnect();
                   },
                   CONNECTION_TIME,
                   0);
    
}

void ChatSession::Init(int port, const std::string& addr, const std::string& name)
{
    uv_tcp_init(&mainloop, &socket);
    uv_ip4_addr(addr.c_str(), port, &dest);
    this->name = name;
    display_line("Init session as " + name + " at " + addr + ":" + std::to_string(port));
    Connect();

    if (main_loop_running == false)
    {
        uv_run(&mainloop, UV_RUN_DEFAULT);
        main_loop_running = true;
    }    
}

const size_t MAX_BUFF_SIZE = 4096;
ChatSession::ChatSession() 
    : main_loop_running(false)
    , connection_handle(nullptr)
    , sending_name(true)
{
    uv_loop_init(&mainloop);
    uv_timer_init(&mainloop, &reconnection_timer);
    uv_timer_init(&mainloop, &connection_timer);
    read_buffer.resize(MAX_BUFF_SIZE);
}

ChatSession::~ChatSession()
{
    uv_loop_close(&mainloop);  
}

std::vector<char>* ChatSession::GetReadBuffer()
{
    return &read_buffer;
}

