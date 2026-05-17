#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <string_view>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string>

using boost::asio::io_context;
using boost::asio::co_spawn;
using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::system::error_code;
using boost::asio::buffer;
using boost::asio::dynamic_buffer;
using boost::asio::transfer_at_least;
using boost::asio::ip::tcp;


constexpr std::string_view delimiter = "\r\n\r\n";
constexpr std::size_t MAX_HEADER_SIZE = 16 * 1024;
constexpr std::size_t BUFFER_SIZE = 8192;


awaitable<void> session(tcp::socket client_socket, io_context& io_context)
{
   try {
        std::string client_request;

        co_await boost::asio::async_read_until(
            client_socket,
            dynamic_buffer(client_request, MAX_HEADER_SIZE),
            delimiter,
            use_awaitable
        );

        auto [host, port] = findHostPort(client_request);

        std::cout << "Client requested host: " << host
                  << ", port: " << port << std::endl;

        tcp::resolver resolver(io_context);

        auto endpoints = co_await resolver.async_resolve(
            host,
            port,
            use_awaitable
        );

        tcp::socket server_socket(io_context);

        co_await boost::asio::async_connect(
            server_socket,
            endpoints,
            use_awaitable
        );

        co_await boost::asio::async_write(
            server_socket,
            buffer(client_request),
            use_awaitable
        );

        if (auto request_content_length = findContentLength(client_request)) 
        {
            std::size_t header_end = client_request.find(std::string(delimiter));

            if (header_end == std::string::npos) 
            {
                throw std::runtime_error("Invalid HTTP request");
            }

            header_end += delimiter.size();

            std::size_t already_have_body = client_request.size() - header_end;

            std::size_t remaining = 0;
            if (*request_content_length > already_have_body) 
            {
                remaining = *request_content_length - already_have_body;
            }

            char data[BUFFER_SIZE];

            while (remaining > 0) {
                std::size_t bytes_to_read = std::min<std::size_t>(
                    sizeof(data),
                    remaining
                );

                std::size_t bytes_read = co_await client_socket.async_read_some(
                    buffer(data, bytes_to_read),
                    use_awaitable
                );

                remaining -= bytes_read;

                co_await boost::asio::async_write(
                    server_socket,
                    buffer(data, bytes_read),
                    use_awaitable
                );
            }
        }

        std::string server_response;

        co_await boost::asio::async_read_until(
            server_socket,
            dynamic_buffer(server_response, MAX_HEADER_SIZE),
            delimiter,
            use_awaitable
        );

        co_await boost::asio::async_write(
            client_socket,
            buffer(server_response),
            use_awaitable
        );

        auto response_content_length = findContentLength(server_response);

        if (!response_content_length.has_value()) 
        {
            char data[BUFFER_SIZE];

            while (true) 
            {
                error_code ec;

                std::size_t bytes_read = co_await server_socket.async_read_some(
                    buffer(data),
                    boost::asio::redirect_error(use_awaitable, ec)
                );

                if (ec == boost::asio::error::eof) 
                {
                    break;
                }

                if (ec) 
                {
                    throw boost::system::system_error(ec);
                }

                co_await boost::asio::async_write(
                    client_socket,
                    buffer(data, bytes_read),
                    use_awaitable
                );
            }
        } 
        else 
        {
            std::size_t header_end = server_response.find(std::string(delimiter));

            if (header_end == std::string::npos) 
            {
                throw std::runtime_error("Invalid HTTP response");
            }

            header_end += delimiter.size();

            std::size_t already_have_body = server_response.size() - header_end;

            std::size_t remaining = 0;
            if (*response_content_length > already_have_body) 
            {
                remaining = *response_content_length - already_have_body;
            }

            char data[BUFFER_SIZE];

            while (remaining > 0) 
            {
                std::size_t bytes_to_read = std::min<std::size_t>(
                    sizeof(data),
                    remaining
                );

                std::size_t bytes_read = co_await server_socket.async_read_some(
                    buffer(data, bytes_to_read),
                    use_awaitable
                );

                remaining -= bytes_read;

                co_await boost::asio::async_write(
                    client_socket,
                    buffer(data, bytes_read),
                    use_awaitable
                );
            }
        }

        client_socket.close();
        server_socket.close();

    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Session error: " << e.what() << std::endl;
    }
}

class Server
{
public:
Server(io_context& io_context, short port)
    : io_context_(io_context)
    , acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    , socket_(io_context)
  {
    do_accept();
  }

private:
    void do_accept() {
        acceptor_.async_accept(
            socket_,
            [this](error_code ec) {
                if (!ec) 
                {
                    std::cout << "Accepted new connection" << std::endl;

                    co_spawn(io_context_, session(std::move(socket_), io_context_), boost::asio::detached);

                    socket_ = tcp::socket(io_context_);
                } 
                else 
                {
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                }

                do_accept();
            }
        );
    }

private:
  io_context& io_context_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
};

int main(int argc, char* argv[]) {
    try 
    {
      if (argc != 2) 
      {
          std::cerr << "Usage: proxy_server <listen_port>\n";
          return 1;
      }

      io_context io_context(1);

      Server server(io_context, static_cast<short>(std::atoi(argv[1])));

      io_context.run();

    } 
    catch (const std::exception& e) 
    {
      std::cerr << "Exception: " << e.what() << std::endl;
      return 1;
    }

    return 0;
}
