//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <beast/core.hpp>
#include <beast/websocket.hpp>
#include <beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <example/common/flat_write_stream.hpp>

namespace ip = boost::asio::ip; // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
namespace http = beast::http; // from <beast/http.hpp>

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <address> <port>\n";
        return EXIT_FAILURE;
    }
    auto address = ip::address::from_string(argv[1]);
    unsigned short port = static_cast<unsigned short>(std::atoi(argv[2]));

    // A helper for reporting errors
    auto const fail =
        [](std::string what, beast::error_code ec)
        {
            std::cerr << what << ": " << ec.message() << std::endl;
            std::cerr.flush();
            return EXIT_FAILURE;
        };

    boost::system::error_code ec;

    // Set up an asio socket to connect to a remote host
    boost::asio::io_service ios;
    tcp::resolver r{ios};
    tcp::socket sock{ios};

    // Make the connection on the IP address we get from a lookup
    sock.connect(tcp::endpoint{address, port}, ec);
    if(ec)
        return fail("connect", ec);

    // Wrap the now-connected socket in an SSL stream
    //using stream_type = ssl::stream<tcp::socket&>;
    using stream_type = boost::asio::ssl::stream<beast::flat_write_stream<boost::asio::ip::tcp::socket&, beast::multi_buffer>>;

    ssl::context ctx{ssl::context::sslv23};
    stream_type stream{sock, ctx};
    stream.set_verify_mode(ssl::verify_none);

    // Perform SSL handshaking
    stream.handshake(ssl::stream_base::client, ec);
    if(ec)
        return fail("ssl handshake", ec);

    beast::multi_buffer b1, b2;
    beast::ostream(b1) <<
        "GET / HTTP/1.1\r\n"
        "Host: [host]\r\n"
        "User-Agent: Beast\r\n"
        "Accept: */*\r\n";
    beast::ostream(b2) <<
        "Connection: Upgrade\r\n"
        "Upgrade: websocket\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    boost::asio::write(stream, beast::buffer_cat(b1.data(), b2.data()), ec);
    if(ec)
        return fail("write", ec);

    http::response<http::string_body> res;
    beast::flat_buffer b;
    http::read(stream, b, res, ec);
    if(ec)
        return fail("read", ec);
    std::cout << res.base() << std::endl;
    return EXIT_SUCCESS;
}
