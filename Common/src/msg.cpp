#include "msg.h"

Msg::Msg(const std::string& str)
: buffer(str.begin(), str.end())
{
    buffer.push_back(0);
    cached_buf.base = buffer.data();
    cached_buf.len = buffer.size();
}

uv_buf_t* Msg::GetBuf()
{
    return &cached_buf;
}

ReqPool::ReqPool()
{
}

uv_write_t* ReqPool::GetNew()
{
    if (unused.empty())
    {
        used.push(std::move(std::unique_ptr<uv_write_t>(new uv_write_t)));
    }
    else
    {
        used.push(std::move(std::unique_ptr<uv_write_t>(unused.front().release())));
        unused.pop();
    }

    return used.back().get();   
}

void ReqPool::Release()
{
    unused.push(std::move(std::unique_ptr<uv_write_t>(used.front().release())));
    used.pop();
}

