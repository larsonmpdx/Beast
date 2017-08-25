//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BEAST_EXAMPLE_COMMON_FLAT_WRITE_STREAM_HPP
#define BEAST_EXAMPLE_COMMON_FLAT_WRITE_STREAM_HPP

#include <boost/beast/config.hpp>
#include <boost/beast/core/async_result.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_service.hpp>
#include <cstdint>
#include <utility>

#include <iostream> //todo: debug only, remove

namespace boost {
namespace beast {

/** A @b Stream with attached @b DynamicBuffer to buffer reads.
    This wraps a @b Stream implementation so that calls to write are
    passed through to the underlying stream, while calls to read will
    first consume the input sequence stored in a @b DynamicBuffer which
    is part of the object.
    The use-case for this class is different than that of the
    `boost::asio::buffered_readstream`. It is designed to facilitate
    the use of `boost::asio::read_until`, and to allow buffers
    acquired during detection of handshakes to be made transparently
    available to callers. A hypothetical implementation of the
    buffered version of `boost::asio::ssl::stream::async_handshake`
    could make use of this wrapper.
    Uses:
    @li Transparently leave untouched input acquired in calls
      to `boost::asio::read_until` behind for subsequent callers.
    @li "Preload" a stream with handshake input data acquired
      from other sources.
    Example:
    @code
    // Process the next HTTP header on the stream,
    // leaving excess bytes behind for the next call.
    //
    template<class DynamicBuffer>
    void process_http_message(
        buffered_write_stream<DynamicBuffer>& stream)
    {
        // Read up to and including the end of the HTTP
        // header, leaving the sequence in the stream's
        // buffer. read_until may read past the end of the
        // headers; the return value will include only the
        // part up to the end of the delimiter.
        //
        std::size_t bytes_transferred =
            boost::asio::read_until(
                stream.next_layer(), stream.buffer(), "\r\n\r\n");
        // Use buffer_prefix() to limit the input
        // sequence to only the data up to and including
        // the trailing "\r\n\r\n".
        //
        auto header_buffers = buffer_prefix(
            bytes_transferred, stream.buffer().data());
        ...
        // Discard the portion of the input corresponding
        // to the HTTP headers.
        //
        stream.buffer().consume(bytes_transferred);
        // Everything we read from the stream
        // is part of the content-body.
    }
    @endcode
    @tparam Stream The type of stream to wrap.
    @tparam DynamicBuffer The type of stream buffer to use.
*/
        template<class Stream>
        class flat_write_stream {
            template<class Buffers, class Handler>
            class write_some_op;

            Stream next_layer_;
            static const std::size_t max_stack_size = 4096;

            /// get length of a buffer sequence
            template<class ConstBufferSequence>
            typename std::enable_if<boost::beast::is_const_buffer_sequence<ConstBufferSequence>::value,
                    std::size_t>::type
            length(ConstBufferSequence const &buffers) {
                return std::distance(buffers.begin(), buffers.end());
            }

            /** Return an iterator to the single non zero buffer if it exists.
                This function returns an iterator to a buffer with a non-zero size if
                both of the following conditions are met:
                @li The buffer sequence has a non zero length
                @li Exactly one buffer in the sequence has a non-zero size
                    Otherwise the function returns `buffers.end()`
                @param buffers The buffer sequence to search through.
            */
            template<class ConstBufferSequence>
            typename ConstBufferSequence::const_iterator
            find_single(ConstBufferSequence const &buffers) {

                using boost::asio::buffer_size;
                auto size = buffer_size(buffers);
                BOOST_ASSERT(size > 0);

                std::cout << "find_single working on " << length(buffers) << " buffers" << std::endl;   //todo: remove

                int num_nonzero = 0;
                auto found_buffer = buffers.end();
                for (auto it = buffers.begin(); it != buffers.end(); it++) {
                    if (buffer_size(*it) > 0) {
                        num_nonzero++;
                        found_buffer = it;
                    }
                }
                if (num_nonzero == 1) {
                    std::cout << "found a single nonzero buffer" << std::endl;   //todo: remove
                    return found_buffer;
                } else {
                    std::cout << "found multiple nonzero buffers" << std::endl;   //todo: remove
                    return buffers.end();
                }
            }

            /// given a buffer sequence, flatten it into a single const_buffers_1
            template<class ConstBufferSequence>
            boost::asio::const_buffers_1
            stack_flatten(ConstBufferSequence const &buffers) {
                char buf[max_stack_size];
                auto copy_size = boost::asio::buffer_copy(boost::asio::buffer(buf, max_stack_size), buffers);
                return boost::asio::const_buffers_1(buf, copy_size);
            }

        public:
            /// The type of the next layer.
            using next_layer_type =
            typename std::remove_reference<Stream>::type;

            /// The type of the lowest layer.
            using lowest_layer_type =
            typename get_lowest_layer<next_layer_type>::type;

            /** Move constructor.
                @note The behavior of move assignment on or from streams
                with active or pending operations is undefined.
            */
            flat_write_stream(flat_write_stream &&) = default;

            /** Move assignment.
                @note The behavior of move assignment on or from streams
                with active or pending operations is undefined.
            */
            flat_write_stream &operator=(flat_write_stream &&) = default;

            /** Construct the wrapping stream.
                @param args Parameters forwarded to the `Stream` constructor.
            */
            template<class... Args>
            explicit
            flat_write_stream(Args &&... args)
                    : next_layer_(std::forward<Args>(args)...)
            {
            }

            /// Get a reference to the next layer.
            next_layer_type &
            next_layer() {
                return next_layer_;
            }

            /// Get a const reference to the next layer.
            next_layer_type const &
            next_layer() const {
                return next_layer_;
            }

            /// Get a reference to the lowest layer.
            lowest_layer_type &
            lowest_layer() {
                return next_layer_.lowest_layer();
            }

            /// Get a const reference to the lowest layer.
            lowest_layer_type const &
            lowest_layer() const {
                return next_layer_.lowest_layer();
            }

            /// Get the io_service associated with the object.
            boost::asio::io_service &
            get_io_service() {
                return next_layer_.get_io_service();
            }

            /** Read some data from the stream.
                This function is used to read data from the stream.
                The function call will block until one or more bytes of
                data has been read successfully, or until an error occurs.
                @param buffers One or more buffers into which the data will be read.
                @return The number of bytes read.
                @throws system_error Thrown on failure.
            */
            template<class MutableBufferSequence>
            std::size_t
            read_some(MutableBufferSequence const &buffers)
            {
                static_assert(is_sync_read_stream<next_layer_type>::value,
                              "SyncReadStream requirements not met");
                return next_layer_.read_some(buffers);
            }

            /** Read some data from the stream.
                This function is used to read data from the stream.
                The function call will block until one or more bytes of
                data has been read successfully, or until an error occurs.
                @param buffers One or more buffers into which the data will be read.
                @param ec Set to the error, if any occurred.
                @return The number of bytes read, or 0 on error.
            */
            template<class MutableBufferSequence>
            std::size_t
            read_some(MutableBufferSequence const &buffers,
                      error_code &ec)
            {
                static_assert(is_sync_read_stream<next_layer_type>::value,
                              "SyncReadStream requirements not met");
                return next_layer_.read_some(buffers, ec);
            }

            /** Start an asynchronous read.
                This function is used to asynchronously read data from
                the stream. The function call always returns immediately.
                @param buffers One or more buffers into which the data
                will be read. Although the buffers object may be copied
                as necessary, ownership of the underlying memory blocks
                is retained by the caller, which must guarantee that they
                remain valid until the handler is called.
                @param handler The handler to be called when the operation
                completes. Copies will be made of the handler as required.
                The equivalent function signature of the handler must be:
                @code void handler(
                    error_code const& error,      // result of operation
                    std::size_t bytes_transferred // number of bytes transferred
                ); @endcode
                Regardless of whether the asynchronous operation completes
                immediately or not, the handler will not be invoked from within
                this function. Invocation of the handler will be performed in a
                manner equivalent to using `boost::asio::io_service::post`.
            */
            template<class MutableBufferSequence, class ReadHandler>
#if BEAST_DOXYGEN
            void_or_deduced
#else
            async_return_type<ReadHandler, void(error_code)>
#endif
            async_read_some(MutableBufferSequence const &buffers,
                            ReadHandler &&handler)
            {
                static_assert(is_async_read_stream<next_layer_type>::value,
                              "Stream requirements not met");
                static_assert(is_mutable_buffer_sequence<
                                      MutableBufferSequence>::value,
                              "MutableBufferSequence requirements not met");
                static_assert(is_completion_handler<ReadHandler,
                                      void(error_code, std::size_t)>::value,
                              "ReadHandler requirements not met");
                return next_layer_.async_read_some(buffers,
                                                   std::forward<ReadHandler>(handler));
            }

            /** Write some data to the stream.
                This function is used to write data to the stream.
                The function call will block until one or more bytes of the
                data has been written successfully, or until an error occurs.
                @param buffers One or more data buffers to be written to the stream.
                @return The number of bytes written.
                @throws system_error Thrown on failure.
            */
            template<class ConstBufferSequence>
            std::size_t
            write_some(ConstBufferSequence const &buffers) {
                static_assert(is_sync_write_stream<next_layer_type>::value,
                              "SyncWriteStream requirements not met");

                error_code ec;
                auto size = write_some(buffers, ec);
                if(ec)
                    BOOST_THROW_EXCEPTION(system_error{ec});
                return size;
            }

            /** Write some data to the stream.
                This function is used to write data to the stream.
                The function call will block until one or more bytes of the
                data has been written successfully, or until an error occurs.
                @param buffers One or more data buffers to be written to the stream.
                @param ec Set to the error, if any occurred.
                @return The number of bytes written.
            */
            template<class ConstBufferSequence>
            std::size_t
            write_some(ConstBufferSequence const &buffers,
                       error_code &ec) {
                static_assert(is_sync_write_stream<next_layer_type>::value,
                              "SyncWriteStream requirements not met");

                auto size = boost::asio::buffer_size(buffers);

                std::cout << "write_some buffer size: " << size << std::endl;   //todo: remove

                if(size > 0) {
                    auto iter = find_single(buffers);
                    if(iter != buffers.end()) {
                        return next_layer_.write_some(boost::asio::const_buffers_1{*iter}, ec);
                    }
                    else {
                        if(size <= max_stack_size) {
                            return next_layer_.write_some(stack_flatten(buffers), ec);
                        } else {
                            try
                            {
                                std::unique_ptr<char[]> buf(new char[size]);
                                auto copy_size = boost::asio::buffer_copy(boost::asio::buffer(buf.get(), size), buffers);
                                return next_layer_.write_some(boost::asio::const_buffers_1(buf.get(), copy_size), ec);
                            }
                            catch(std::bad_alloc const&)
                            {
                                ec = http::error::bad_alloc;
                                return 0;
                            }
                        }
                    }
                }
                else {
                    return next_layer_.write_some(buffers, ec);
                }
            }

            /** Start an asynchronous write.
                This function is used to asynchronously write data from
                the stream. The function call always returns immediately.
                @param buffers One or more data buffers to be written to
                the stream. Although the buffers object may be copied as
                necessary, ownership of the underlying memory blocks is
                retained by the caller, which must guarantee that they
                remain valid until the handler is called.
                @param handler The handler to be called when the operation
                completes. Copies will be made of the handler as required.
                The equivalent function signature of the handler must be:
                @code void handler(
                    error_code const& error,      // result of operation
                    std::size_t bytes_transferred // number of bytes transferred
                ); @endcode
                Regardless of whether the asynchronous operation completes
                immediately or not, the handler will not be invoked from within
                this function. Invocation of the handler will be performed in a
                manner equivalent to using `boost::asio::io_service::post`.
            */
            template<class ConstBufferSequence, class WriteHandler>
#if BEAST_DOXYGEN
            void_or_deduced
#else
            async_return_type<WriteHandler, void(error_code)>
#endif
            async_write_some(ConstBufferSequence const &buffers,
                             WriteHandler &&handler)
            {
                static_assert(is_async_write_stream<next_layer_type>::value,
                              "AsyncWriteStream requirements not met");
                static_assert(is_const_buffer_sequence<
                                      ConstBufferSequence>::value,
                              "ConstBufferSequence requirements not met");
                static_assert(is_completion_handler<WriteHandler,
                                      void(error_code, std::size_t)>::value,
                              "WriteHandler requirements not met");

                auto size = boost::asio::buffer_size(buffers);

                std::cout << "async_write_some buffer size:: " << size << std::endl;   //todo: remove


                if(size > 0) {
                    auto iter = find_single(buffers);
                    if(iter != buffers.end()) {
                        return next_layer_.async_write_some(boost::asio::const_buffers_1{*iter},
                                                     std::forward<WriteHandler>(handler));
                    }
                    else {
                        if(size <= max_stack_size) {
                            return next_layer_.async_write_some(stack_flatten(buffers),
                                                         std::forward<WriteHandler>(handler));
                        } else {
                            try
                            {
                                std::unique_ptr<char[]> buf(new char[size]);
                                auto copy_size = boost::asio::buffer_copy(boost::asio::buffer(buf.get(), size), buffers);
                                return next_layer_.async_write_some(boost::asio::const_buffers_1(buf.get(), copy_size),
                                                             std::forward<WriteHandler>(handler));
                            }
                            catch(std::bad_alloc const&)
                            {
                                return; // todo: handle error?
                            }
                        }
                    }
                }
                else {
                    return next_layer_.async_write_some(buffers,
                                                        std::forward<WriteHandler>(handler));
                }
            }

            friend
            void
            teardown(websocket::role_type,
                     flat_write_stream&, boost::system::error_code& ec)
            {
                ec.assign(0, ec.category());
            }

            template<class TeardownHandler>
            friend
            void
            async_teardown(websocket::role_type,
                           flat_write_stream& s, TeardownHandler&& handler)
            {
                std::cout << "async_teardown" << std::endl;   //todo: remove

                s.get_io_service().post(
                        bind_handler(std::move(handler),
                                     error_code{}));
            }
        };

        template<class Stream>
        template<class MutableBufferSequence, class Handler>
        class flat_write_stream<
                Stream>::write_some_op
        {
            int step_ = 0;
            flat_write_stream& s_;
            MutableBufferSequence b_;
            Handler h_;

        public:
            write_some_op(write_some_op&&) = default;
            write_some_op(write_some_op const&) = default;

            template<class DeducedHandler, class... Args>
            write_some_op(DeducedHandler&& h,
                          flat_write_stream& s,
                          MutableBufferSequence const& b)
                    : s_(s)
                    , b_(b)
                    , h_(std::forward<DeducedHandler>(h))
            {
            }

            void
            operator()(error_code const& ec,
                       std::size_t bytes_transferred);

            friend
            void* asio_handler_allocate(
                    std::size_t size, write_some_op* op)
            {

                std::cout << "allocate" << std::endl;   //todo: remove

                using boost::asio::asio_handler_allocate;
                return asio_handler_allocate(
                        size, std::addressof(op->h_));
            }

            friend
            void asio_handler_deallocate(
                    void* p, std::size_t size, write_some_op* op)
            {
                using boost::asio::asio_handler_deallocate;
                asio_handler_deallocate(
                        p, size, std::addressof(op->h_));
            }

            friend
            bool asio_handler_is_continuation(write_some_op* op)
            {
                using boost::asio::asio_handler_is_continuation;
                return asio_handler_is_continuation(
                        std::addressof(op->h_));
            }

            template<class Function>
            friend
            void asio_handler_invoke(Function&& f, write_some_op* op)
            {
                using boost::asio::asio_handler_invoke;
                asio_handler_invoke(f, std::addressof(op->h_));
            }
        };

        template<class Stream>
        template<class MutableBufferSequence, class Handler>
        void
        flat_write_stream<Stream>::
        write_some_op<MutableBufferSequence, Handler>::operator()(
                error_code const& ec, std::size_t bytes_transferred)
        {
            std::cout << "write_some_op" << std::endl;   //todo: remove
        }

    }
}

#endif
