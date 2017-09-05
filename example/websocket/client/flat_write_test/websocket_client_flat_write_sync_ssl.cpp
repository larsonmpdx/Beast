//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <example/common/flat_write_stream.hpp>

namespace ip = boost::asio::ip; // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
namespace websocket = boost::beast::websocket; // from <beast/websocket.hpp>

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 4)
    {
        std::cerr <<
                  "Usage: websocket-client-coro-ssl <host> <port> <text>\n" <<
                  "Example:\n" <<
                  "    websocket-client-coro-ssl echo.websocket.org 443 \"Hello, world!\"\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const text = argv[3];

    // A helper for reporting errors
    auto const fail =
            [](std::string what, boost::beast::error_code ec)
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

    // Look up the domain name
    auto const lookup = r.resolve({host, port}, ec);
    if(ec)
        return fail("resolve", ec);

    // Make the connection on the IP address we get from a lookup
    boost::asio::connect(sock, lookup, ec);
    if(ec)
        return fail("connect", ec);

    // Wrap the now-connected socket in an SSL stream
    //using stream_type = ssl::stream<tcp::socket&>;
    using stream_type = boost::beast::flat_write_stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>>;

    ssl::context ctx{ssl::context::sslv23};
    stream_type stream{sock, ctx};
    stream.next_layer().set_verify_mode(ssl::verify_none);

    // Perform SSL handshaking
    stream.next_layer().handshake(ssl::stream_base::client, ec);
    if(ec)
        return fail("ssl handshake", ec);

    // Now wrap the handshaked SSL stream in a websocket stream
    boost::beast::websocket::stream<stream_type &> ws{stream};

    // Perform the websocket handshake
    boost::beast::websocket::response_type res;
    ws.handshake(res, host, "/", ec);
    if(ec) {
        return fail("handshake", ec);
    }

    // Send a message
    ws.write(boost::asio::buffer(std::string(text)), ec);
    if(ec)
        return fail("write", ec);

    // This buffer will hold the incoming message
    boost::beast::multi_buffer b;

    // Read the message into our buffer
    ws.read(b, ec);
    if(ec)
        return fail("read", ec);

    // Send a "close" frame to the other end, this is a websocket thing
    ws.close(websocket::close_code::normal, ec);
    if(ec)
        return fail("close", ec);

    // The buffers() function helps print a ConstBufferSequence
    std::cout << boost::beast::buffers(b.data()) << std::endl;

    // If we get here the connection was cleanly closed
    return EXIT_SUCCESS;
}