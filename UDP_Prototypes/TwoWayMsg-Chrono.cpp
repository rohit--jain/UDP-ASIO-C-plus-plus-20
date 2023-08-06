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

awaitable<void> client_5(io_context& ctx)
{
    auto endpoint = ip::udp::endpoint(ip::make_address_v4("127.0.0.1"), 4242);
    auto client_endpoint = ip::udp::endpoint(ip::make_address_v4("127.0.0.1"), 4243);
    auto socket = ip::udp::socket(ctx, client_endpoint);

    ip::udp::endpoint server_endpoint;
    std::array<std::byte, 1000> server_response_buffer;
    buffer(server_response_buffer.data(), server_response_buffer.size());

    Data server_request_data{ 42, "foo" };
    for (int j = 0; j < 9; j++)
    {
        uint64_t number_bigEndian = htonl(j);
        // uint64_t number_bigEndian = htobe64(server_request_data.number); // Linux Version

        uint32_t text_size_bigEndian = htonl(static_cast<uint32_t>(server_request_data.text.size()));
        //uint32_t text_size_bigEndian = htobe32(static_cast<uint32_t>(server_request_data.text.size())); // Linux Version
        co_await socket.async_send_to(std::array<const_buffer, 3>{
            buffer(&number_bigEndian, sizeof(number_bigEndian)),
                buffer(&text_size_bigEndian, sizeof(text_size_bigEndian)),
                buffer(server_request_data.text)
        }, endpoint, use_awaitable);
        
		std::this_thread::sleep_for(1s);

        auto num_bytes = co_await socket.async_receive_from(mutable_buffer(server_response_buffer.data(), server_response_buffer.size()),
                                                            server_endpoint, use_awaitable);
        size_t client_cursor = 0;
        Data server_response_data;
        server_response_data.number = ntohl(*(uint64_t*)(&server_response_buffer[client_cursor]));
        client_cursor += sizeof(server_response_data.number);
        uint32_t server_response_string_size = ntohl(*(uint32_t*)(&server_response_buffer[client_cursor]));
        client_cursor += sizeof(server_response_string_size);
        server_response_data.text = std::string((char*)(&server_response_buffer[client_cursor]), server_response_string_size);

        std::cout << "[CLIENT] Server Ack Number: " << server_response_data.number << " Server Ack Text: " << server_response_data.text << std::endl;
    }

}


awaitable<void> server_5(io_context& ctx)
{
    auto server_endpoint = ip::udp::endpoint(ip::make_address_v4("127.0.0.1"), 4242);
    auto socket = ip::udp::socket(ctx, server_endpoint);

    ip::udp::endpoint client_endpoint;
    std::array<std::byte, 1000> input_buffer;
    buffer(input_buffer.data(), input_buffer.size());
	
	 Data client_response_data{ 42, "foo" };
	
    for(int i = 0;i < 9;i++) 
    {
        auto num_bytes = co_await socket.async_receive_from(mutable_buffer(input_buffer.data(), input_buffer.size()),
            client_endpoint, use_awaitable);
        size_t cursor = 0;
        
        Data client_request_data;
        client_request_data.number = ntohl(*(uint64_t*)(&input_buffer[cursor]));
        //client_request_data.number = be64toh(*(uint64_t*)(&input_buffer[cursor])); // Linux Version
        cursor += sizeof(client_request_data.number);
        uint32_t string_size = ntohl(*(uint32_t*)(&input_buffer[cursor]));
        //uint32_t string_size = be32toh(*(uint32_t*)(&input_buffer[cursor])); // Linux Version
        cursor += sizeof(string_size);
        client_request_data.text = std::string((char*)(&input_buffer[cursor]), string_size);

        std::cout << "[SERVER] Request Frame Number: " << client_request_data.number << " Request Frame Text: " << client_request_data.text << std::endl;
		
		//std::this_thread::sleep_for(1s);
		
		std::string response_text = "#Ack+" + client_request_data.text + "=" + std::to_string(client_request_data.number);
		
		uint64_t number_bigEndian = htonl(client_request_data.number + 1000);
        // uint64_t number_bigEndian = htobe64(client_response_data.number); // Linux Version

        uint32_t text_size_bigEndian = htonl(static_cast<uint32_t>(response_text.size()));
        //uint32_t text_size_bigEndian = htobe32(static_cast<uint32_t>(response_text.size())); // Linux Version
        co_await socket.async_send_to(std::array<const_buffer, 3>{
            buffer(&number_bigEndian, sizeof(number_bigEndian)),
                buffer(&text_size_bigEndian, sizeof(text_size_bigEndian)),
                buffer(response_text)
        }, client_endpoint, use_awaitable);


    }
    std::cout << "[SERVER] Server Loop Finished" << std::endl;
}


int main() {

    io_context ctx;

    std::cout<< "[SERVER] starting server..."<< std::endl;
    co_spawn(ctx, server_5(ctx), detached);

    std::cout<< "[CLIENT] starting client..."<< std::endl;
    co_spawn(ctx, client_5(ctx), detached);


    ctx.run();

    return 0;
}
