#pragma once

#include <uv.h>
#include <vector>
#include <string>
#include <memory>
#include <queue>

typedef std::vector<char> msg_buffer;

struct MsgReq
{
    uv_write_t request;
};

class Msg
{
public:
    Msg(const std::string& str);
    uv_buf_t* GetBuf();
protected:
    msg_buffer buffer;
    uv_buf_t cached_buf;

    // just need to save them
    // those are unused
};

class ReqPool
{
public:
    ReqPool();
    uv_write_t* GetNew();
    void Release();
protected:
    std::queue<std::unique_ptr<uv_write_t>>unused;
    std::queue<std::unique_ptr<uv_write_t>>used;
};

class MsgPool
{
public:
    std::queue<std::shared_ptr<Msg>> messages;
    ReqPool requests; 
};
