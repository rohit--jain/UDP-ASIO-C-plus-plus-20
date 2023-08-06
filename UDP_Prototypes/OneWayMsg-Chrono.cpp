#include <iostream>
#include <string>
#include <asio.hpp>
#include <array>
#include <chrono>
#include <thread>

using namespace asio;
using namespace std::literals::chrono_literals;

struct Data {
    uint64_t number;
    std::string text;
};

awaitable<void> client(io_context& ctx)
{
    auto endpoint = ip::udp::endpoint(ip::make_address_v4("127.0.0.1"), 4242);
    auto client_endpoint = ip::udp::endpoint(ip::make_address_v4("127.0.0.1"), 4243);
    auto socket = ip::udp::socket(ctx, client_endpoint);

    Data data{ 42, "foo" };
    for (int j = 0; j < 10; j++) 
    {
        uint64_t number_be = htonl(j);
        // uint64_t number_be = htobe64(data.number); // Linux Version

        uint32_t text_size_be = htonl(static_cast<uint32_t>(data.text.size()));
        //uint32_t text_size_be = htobe32(static_cast<uint32_t>(data.text.size())); // Linux Version
        co_await socket.async_send_to(std::array<const_buffer, 3>{
            buffer(&number_be, sizeof(number_be)),
                buffer(&text_size_be, sizeof(text_size_be)),
                buffer(data.text)
        }, endpoint, use_awaitable);
        std::this_thread::sleep_for(1s);

    }
}


awaitable<void> server(io_context& ctx)
{
    auto endpoint = ip::udp::endpoint(ip::make_address_v4("127.0.0.1"), 4242);
    auto socket = ip::udp::socket(ctx, endpoint);

    ip::udp::endpoint sender_endpoint;
    std::array<std::byte, 1000> input_buffer;
    buffer(input_buffer.data(), input_buffer.size());
    for(int i = 0;i < 9;i++) 
    {
        auto num_bytes = co_await socket.async_receive_from(mutable_buffer(input_buffer.data(), input_buffer.size()),
            sender_endpoint, use_awaitable);
        size_t cursor = 0;
        
        Data data;
        data.number = ntohl(*(uint64_t*)(&input_buffer[cursor]));
        //data.number = be64toh(*(uint64_t*)(&input_buffer[cursor])); // Linux Version
        cursor += sizeof(data.number);
        uint32_t string_size = ntohl(*(uint32_t*)(&input_buffer[cursor]));
        //uint32_t string_size = be32toh(*(uint32_t*)(&input_buffer[cursor])); // Linux Version
        cursor += sizeof(string_size);
        data.text = std::string((char*)(&input_buffer[cursor]), string_size);

        std::cout << "data.number: " << data.number << "\ndata.text: " << data.text << std::endl;
    }
    std::cout << "Server Down" << std::endl;
}


int main() {

    io_context ctx;

    co_spawn(ctx, client(ctx), detached);
    co_spawn(ctx, server(ctx), detached);

    ctx.run();

    return 0;
}
