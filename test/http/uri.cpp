//
// Copyright (c) 2013-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Test that header file is self-contained.
//#include <beast/http/uri.hpp>

#include <beast/core/error.hpp>
#include <beast/core/string.hpp>
#include <beast/http/error.hpp>
#include <beast/unit_test/suite.hpp>
#include <boost/lexical_cast.hpp>
#include <cstring>

/*
    Uniform Resource Identifier (URI): Generic Syntax
    https://tools.ietf.org/html/rfc3986

    Internationalized Resource Identifiers (IRIs)
    https://tools.ietf.org/html/rfc3987

    Hypertext Transfer Protocol (HTTP/1.1): Semantics and Content
    https://tools.ietf.org/html/rfc7231
*/

namespace beast {
namespace http {

//------------------------------------------------------------------------------

struct uri
{
    enum class error
    {
        /// An input did not match a structural element (soft error)
        mismatch = 1,

        /// A syntax error occurred
        syntax,

        /// The parser encountered an invalid input
        invalid
    };
};

} // http
} // beast

namespace boost {
namespace system {
template<>
struct is_error_code_enum<beast::http::uri::error>
{
    static bool const value = true;
};
} // system
} // boost

namespace beast {
namespace http {

namespace detail {

class uri_error_category : public error_category
{
public:
    const char*
    name() const noexcept override
    {
        return "beast.http.uri";
    }

    std::string
    message(int ev) const override
    {
        switch(static_cast<error>(ev))
        {
        case uri::error::mismatch: return "mismatched element";
        case uri::error::syntax: return "syntax error";
        case uri::error::invalid: return "invalid input";
        default:
            return "beast.http.uri error";
        }
    }

    error_condition
    default_error_condition(
        int ev) const noexcept override
    {
        return error_condition{ev, *this};
    }

    bool
    equivalent(int ev,
        error_condition const& condition
            ) const noexcept override
    {
        return condition.value() == ev &&
            &condition.category() == this;
    }

    bool
    equivalent(error_code const& error,
        int ev) const noexcept override
    {
        return error.value() == ev &&
            &error.category() == this;
    }
};

inline
error_category const&
get_uri_error_category()
{
    static uri_error_category const cat{};
    return cat;
}

} // detail

inline
error_code
make_error_code(uri::error ev)
{
    return error_code{
        static_cast<std::underlying_type<error>::type>(ev),
            detail::get_uri_error_category()};
}

//------------------------------------------------------------------------------

class uri_view
{
    char const* base_;

protected:
    static
    std::size_t
    dist(char const* first, char const* last)
    {
        return static_cast<
            std::size_t>(last - first);
    }

    struct str_t
    {
        unsigned short offset;
        unsigned short length;

        str_t(str_t const&) = default;

        str_t()
            : offset(0)
            , length(0)
        {
        }

        str_t(
            char const* first,
            char const* last,
            char const* base)
            : offset(static_cast<
                unsigned short>(first - base))
            , length(static_cast<
                unsigned short>(last - first))
        {
        }

        bool
        empty() const
        {
            return length == 0;
        }

        char*
        data(char* base) const
        {
            return base + offset;
        }

        string_view
        operator()(char const* base) const
        {
            return {base + offset, length};
        }
    };

    str_t str_;
        str_t scheme_;
        str_t authority_;
            str_t userinfo_;
            str_t host_;
            str_t port_;
        str_t path_;
        str_t query_;
        str_t fragment_;

    explicit
    uri_view(char const* base)
        : base_(base)
    {
    }

    void
    reset()
    {
        str_ = {};
            scheme_ = {};
            authority_ = {};
                userinfo_ = {};
                host_ = {};
                port_ = {};
            path_ = {};
            query_ = {};
            fragment_ = {};
    }

public:
    string_view
    str() const
    {
        return str_(base_);
    }

    string_view
    scheme() const
    {
        return scheme_(base_);
    }

    string_view
    authority() const
    {
        return authority_(base_);
    }

    string_view
    userinfo() const
    {
        return userinfo_(base_);
    }

    string_view
    host() const
    {
        return host_(base_);
    }

    string_view
    port() const
    {
        return port_(base_);
    }

    string_view
    path() const
    {
        return path_(base_);
    }

    string_view
    query() const
    {
        return query_(base_);
    }

    string_view
    fragment() const
    {
        return fragment_(base_);
    }
};

//------------------------------------------------------------------------------

class uri_base : public uri_view
{
protected:
    char* base_;
    char* first_;
    char* last_;
    char* end_;

    uri_base(char* base, std::size_t size)
        : uri_view(base)
        , base_(base)
        , first_(base_)
        , last_(base_)
        , end_(base_ + size)
    {
    }

    // append a char to the current string
    void
    append(char c)
    {
        if(last_ >= end_)
            BOOST_THROW_EXCEPTION(
                std::length_error{"uri overflow"});
        *last_++ = c;
    }

    // append a percent encoded char to the
    // current string. the hex digits are normalized
    void
    append_pct_encoded(char c)
    {
        if(last_ + 3 > end_)
            BOOST_THROW_EXCEPTION(
                std::length_error{"uri overflow"});
        *last_++ = '%';
        *last_++ = hex_digit(
            static_cast<unsigned char>(c) / 16);
        *last_++ = hex_digit(
            static_cast<unsigned char>(c) % 16);
    }

    // mark the beginning of the current string
    void
    mark()
    {
        first_ = last_;
    }

    // get the current string, start a new one
    str_t
    extract()
    {
        str_t const s{first_, last_, base_};
        first_ = last_;
        return s;
    }

    //--------------------------------------------------------------------------

    char
    ascii_tolower(char c)
    {
        if(c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        return c;
    }

    static
    bool
    is_alpha(char c)
    {
        return
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z');
    }

    static
    bool
    is_digit(char c)
    {
        return  c >= '0' && c <= '9';
    }

    static
    bool
    is_alnum(char c)
    {
        return is_alpha(c) || is_digit(c);
    }

    static
    bool
    is_reserved(char c)
    {
    /*
        reserved    = gen-delims / sub-delims

        gen-delims  = ":" / "/" / "?" / "#" / "[" / "]" / "@"

        sub-delims  = "!" / "$" / "&" / "'" / "(" / ")"
                          / "*" / "+" / "," / ";" / "="
    */
        switch(c)
        {
        case ':': case '/': case '?': case '#':  case '[': case ']':
        case '!': case '$': case '&': case '\'': case '(': case ')':
        case '*': case '+': case ',': case ';':  case '=':
            return true;
        }
        return false;
    }

    //--------------------------------------------------------------------------

    static
    bool
    is_unreserved(char c)
    {
    /*
        unreserved      = ALPHA / DIGIT / "-" / "." / "_" / "~"
    */
        return
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
             c == '-' || c == '.'  ||
             c == '_' || c == '~'
            ;
    } 

    static
    bool
    is_sub_delim(char c)
    {
    /*
        sub-delims      = "!" / "$" / "&" / "'" / "(" / ")"
                              / "*" / "+" / "," / ";" / "="
    */
        return
            c == '!' || c == '$' || c == '&' || c == '\'' ||
            c == '(' || c == ')' || c == '*' || c == '+'  ||
            c == ',' || c == ';' || c == '='
            ;
    }

    static
    int
    hex_val(char c)
    {
        if(c >= '0' && c <= '9')
            return c - '0';
        if(c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if(c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    }

    static
    char
    hex_digit(int v)
    {
        if(v < 10)
            return static_cast<char>(
                '0' + v);
        return static_cast<char>(
            'A' + v - 10);
    }

    void
    parse_pct_encoded(
        char const*& in,
        char const* last,
        char& c,
        error_code& ec)
    {
    /*
        pct-encoded     = "%" HEXDIG HEXDIG
    */
        BOOST_ASSERT(in < last);
        if(*in != '%')
        {
            ec = uri::error::mismatch;
            return;
        }
        if(in + 3 > last)
        {
            // short input
            ec = uri::error::syntax;
            return;
        }
        auto d1 = hex_val(in[1]);
        if(d1 == -1)
        {
            // invalid hex digit
            ec = uri::error::invalid;
            return;
        }
        auto d2 = hex_val(in[2]);
        if(d2 == -1)
        {
            // invalid hex digit
            ec = uri::error::invalid;
            return;
        }
        c = static_cast<char>(
            16 * d1 + d2);
        in += 3;
    }

    void
    parse_pchar(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        pchar           = unreserved / pct-encoded / sub-delims / ":" / "@"
    */
        BOOST_ASSERT(in < last);
        if(is_unreserved(*in))
        {
            append(*in++);
            return;
        }
        char c;
        parse_pct_encoded(in, last, c, ec);
        if(! ec)
        {
            if(is_unreserved(c))
            {
                append(c);
            }
            else
            {
                /*  VFALCO Small problem here
                    https://tools.ietf.org/html/rfc3986#section-2.2
                    URI producing applications should percent-encode data octets that
                    correspond to characters in the reserved set unless these characters
                    are specifically allowed by the URI scheme to represent data in that
                    component.

                    How do we know if the URI scheme considers `c` a delimiter?
                    Maybe we need to use CRTP or something as a customization point.
                */
                append_pct_encoded(c);
            }
            return;
        }
        if(ec != uri::error::mismatch)
            return;

        if(is_sub_delim(*in))
        {
            append(*in++);
            ec.assign(0, ec.category());
            return;
        }

        if(*in == ':' || *in == '@')
        {
            append(*in++);
            ec.assign(0, ec.category());
            return;
        }

        ec = uri::error::mismatch;
    }

    void
    parse_absolute_path(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        absolute-path   = 1*( "/" segment )

        segment         = *pchar
    */
        BOOST_ASSERT(in < last);
        if(*in != '/')
        {
            // expected '/'
            ec = uri::error::syntax;
            return;
        }
        goto start;
        for(;;)
        {
            if(in >= last)
                break;
            if(*in == '/')
            {
            start:
                append(*in++);
                continue;
            }
            parse_pchar(in, last, ec);
            if(ec == uri::error::mismatch)
                break;
            if(ec)
                return;
        }
        ec.assign(0, ec.category());
        path_ = extract();
    }

    void
    parse_query(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        query       = *( pchar / "/" / "?" )
    */
        mark();
        while(in < last)
        {
            parse_pchar(in, last, ec);
            if(ec == uri::error::mismatch)
            {
                ec.assign(0, ec.category());
                if(*in != '/' && *in != '?')
                    break;
                append(*in++);
            }
            else if(ec)
            {
                return;
            }
        }
        query_ = extract();
    }

    //--------------------------------------------------------------------------

    void
    parse_origin_form(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        origin-form     = absolute-path [ "?" query ]
    */
        BOOST_ASSERT(in < last);
        if(*in != '/')
        {
            ec = uri::error::mismatch;
            return;
        }
        parse_absolute_path(in, last, ec);
        if(ec)
            return;
        if(in >= last)
            return;
        if(*in++ != '?')
        {
            // expected '?'
            ec = uri::error::syntax;
            return;
        }
        append('?');
        parse_query(in, last, ec);
        if(ec)
            return;
    }

    //--------------------------------------------------------------------------

    void
    parse_scheme(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        scheme        = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    */
        BOOST_ASSERT(in < last);
        if(! is_alpha(*in))
        {
            ec = uri::error::mismatch;
            return;
        }
        for(;;)
        {
            append(ascii_tolower(*in++));
            if(in >= last)
                break;
            if( ! is_alnum(*in) &&
                *in != '+' &&
                *in != '-' &&
                *in != '.')
                break;
        }
        scheme_ = extract();
    }

    void
    parse_userinfo(
        char const*& in, char const* last,
            error_code& ec)
    {
    }

    void
    parse_ipv6_address(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        IPv6address     =                            6( h16 ":" ) ls32
                        /                       "::" 5( h16 ":" ) ls32
                        / [               h16 ] "::" 4( h16 ":" ) ls32
                        / [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
                        / [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
                        / [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
                        / [ *4( h16 ":" ) h16 ] "::"              ls32
                        / [ *5( h16 ":" ) h16 ] "::"              h16
                        / [ *6( h16 ":" ) h16 ] "::"

        h16             = 1*4HEXDIG
        ls32            = ( h16 ":" h16 ) / IPv4address
    */
        // VFALCO TODO
        ec = uri::error::mismatch;
    }

    void
    parse_ipv_future(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        IPvFuture       = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
    */
        BOOST_ASSERT(in < last);
        BOOST_ASSERT(*in == 'v');
        
        // VFALCO TODO
        ec = uri::error::mismatch;
    }

    void
    parse_ip_literal(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        IP-literal      = "[" ( IPv6address / IPvFuture  ) "]"
    */
        BOOST_ASSERT(in < last);
        BOOST_ASSERT(*in == '[');
        if(in + 6 > last)
        {
            // too short to be real
            ec = uri::error::syntax;
            return;
        }
        append(*in++);
        if(*in == 'v')
        {
            append('v');
            mark();
            parse_ipv_future(in, last, ec);
            if(ec)
                return;
        }
        else
        {
            mark();
            parse_ipv6_address(in, last, ec);
            if(ec)
                return;
        }
        if(in + 1 > last || *in != ']')
        {
            // expected ']'
            ec = uri::error::syntax;
            return;
        }
        host_ = extract();
        append(']');
    }

    void
    parse_ipv4_address(
        char const*& in, char const* last,
            error_code& ec)
    {
        char const* p = in;
        auto const parse_dec_octet =
            [&]()
            {
                if(p >= last || ! is_digit(*p))
                {
                    // expected DIGIT
                    ec = uri::error::mismatch;
                    return;
                }
                unsigned v = *p++ - '0';
                if(p >= last || ! is_digit(*p))
                    return;
                v = 10 * v + *p++ - '0';
                if(p >= last || ! is_digit(*p))
                    return;
                v = 10 * v + *p++ - '0';
                if(v > 255)
                {
                    // expected dec-octet
                    ec = uri::error::mismatch;
                    return;
                }
            };
        parse_dec_octet();
        if(ec)
            return;
        if(p >= last || *p++ != '.')
        {
            // expected '.'
            ec = uri::error::mismatch;
            return;
        }
        parse_dec_octet();
        if(ec)
            return;
        if(p >= last || *p++ != '.')
        {
            // expected '.'
            ec = uri::error::mismatch;
            return;
        }
        parse_dec_octet();
        if(ec)
            return;
        if(p >= last || *p++ != '.')
        {
            // expected '.'
            ec = uri::error::mismatch;
            return;
        }
        parse_dec_octet();
        if(ec)
            return;
        while(in < p)
            append(*in++);
    }

    void
    parse_reg_name(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        reg-name        = *( unreserved / pct-encoded / sub-delims )
    */
        while(in < last)
        {
            if( ! is_unreserved(*in) &&
                ! is_sub_delim(*in))
            {
                char c;
                parse_pct_encoded(in, last, c, ec);
                if(ec == uri::error::mismatch)
                {
                    ec.assign(0, ec.category());
                    break;
                }
                append(c);
            }
            else
            {
                append(*in++);
            }
        }
    }

    void
    parse_host(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        host            = IP-literal / IPv4address / reg-name

        https://tools.ietf.org/html/rfc3986#section-3.2.2
        The syntax rule for host is ambiguous because it does not completely
        distinguish between an IPv4address and a reg-name.  In order to
        disambiguate the syntax, we apply the "first-match-wins" algorithm:
        If host matches the rule for IPv4address, then it should be
        considered an IPv4 address literal and not a reg-name.  Although host

        Minimum sizes:
            IP-literal      = 6 = 1 + 4 + 1
            IPv4address     = 7 = 1 + 1 + 1 + 1 + 1 + 1 + 1
            reg-name        = 0
    */
        if(in + 1 <= last && *in == '[')
        {
            parse_ip_literal(in, last, ec);
            return;
        }
        parse_ipv4_address(in, last, ec);
        if(ec != uri::error::mismatch)
        {
            host_ = extract();
            return;
        }
        ec.assign(0, ec.category());
        parse_reg_name(in, last, ec);
        if(ec)
            return;
        host_ = extract();
    }

    void
    parse_authority(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        authority       = [ userinfo "@" ] host [ ":" port ]
        userinfo        = *( unreserved / pct-encoded / sub-delims / ":" )

        port            = *DIGIT
    */
        auto const at = reinterpret_cast<char const*>(
            std::memchr(in, '@', static_cast<
                std::size_t>(last - in)));
        if(at)
        {
#if 0
            parse_userinfo(in, last, ec);
            if(ec)
                return;
            if(in != at)
            {
                // expected '@'
                ec = uri::error::syntax;
                return;
            }
            append(*in++);
#endif
        }
        parse_host(in, last, ec);
        if(ec)
            return;
    }

    void
    parse_hier_part(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        hier-part       = "//" authority path-abempty
                        / path-absolute
                        / path-rootless
                        / path-empty

        path            = path-abempty    ; begins with "/" or is empty
                        / path-absolute   ; begins with "/" but not "//"
                        / path-noscheme   ; begins with a non-colon segment
                        / path-rootless   ; begins with a segment
                        / path-empty      ; zero characters

        path-abempty    = *( "/" segment )
        path-absolute   = "/" [ segment-nz *( "/" segment ) ]
        path-noscheme   = segment-nz-nc *( "/" segment )
        path-rootless   = segment-nz *( "/" segment )
        path-empty      = 0<pchar>

        segment         = *pchar
        segment-nz      = 1*pchar
        segment-nz-nc   = 1*( unreserved / pct-encoded / sub-delims / "@" )
                          ; non-zero-length segment without any colon ":"
    */
        if(in + 2 > last)
        {
            // expected '::'
            ec = uri::error::syntax;
            return;
        }
        if(in[0] != ':' || in[1] != ':')
        {
            // expected '::'
            ec = uri::error::syntax;
            return;
        }
        append(*in++);
        append(*in++);
        parse_authority(in, last, ec);
        if(ec)
            return;
        if(in >= last)
        {
            // path-empty
            return;
        }

    }

    void
    parse_absolute_form(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        absolute-form   = scheme ":" hier-part [ "?" query ]
    */
        parse_scheme(in, last, ec);
        if(ec)
            return;
        if(in >= last)
        {
            // missing elements
            ec = uri::error::invalid;
            return;
        }
        if(*in != ':')
        {
            // syntax
            ec = uri::error::syntax;
            return;
        }
        append(*in++);
        if(in >= last)
        {
            // missing elements
            ec = uri::error::invalid;
            return;
        }
        parse_hier_part(in, last, ec);
        if(ec)
            return;
        if(in < last)
        {
            if(*in != '?')
                return;
            append(*in++);
        }
        parse_query(in, last, ec);
    }

    //--------------------------------------------------------------------------

    void
    parse_authority_form(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        The authority-form of request-target is only used for CONNECT requests
        https://tools.ietf.org/html/rfc7230#section-5.3.3

        Although CONNECT must exclude userinfo and '@' we parse it anyway and
        let the caller decide what to do with it.

        authority-form  = authority
    */
        parse_authority(in, last, ec);
    }

    //--------------------------------------------------------------------------

    void
    parse_asterisk_form(
        char const*& in, char const* last,
            error_code& ec)
    {
    /*
        asterisk-form   = "*"
    */
        BOOST_ASSERT(in < last);
        if(*in != '*')
        {
            ec = uri::error::mismatch;
            return;
        }
        ++in;
        if(in != last)
        {
            ec = uri::error::invalid;
            return;
        }
        append('*');
        path_ = extract();
        ec.assign(0, ec.category());
    }


public:
    void
    reset()
    {
        first_ = base_;
        last_ = base_;
        uri_view::reset();
    }

    void
    parse_uri(string_view s, error_code& ec)
    {
    /*
        URI           = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
    */
    }

    void
    parse_request_target(
        string_view s, error_code& ec)
    {
    /*
        request-target  = origin-form
                        / absolute-form
                        / authority-form
                        / asterisk-form
    */
        auto in = s.data();
        auto const last = s.end();
        if(in >= last)
        {
            // empty string
            ec = http::error::bad_target;
            return;
        }
        for(;;)
        {
            parse_origin_form(in, last, ec);
            if(! ec)
                break;
            if(ec != uri::error::mismatch)
                return;
            ec.assign(0, ec.category());

            parse_absolute_form(in, last, ec);
            if(! ec)
                break;
            if(ec != uri::error::mismatch)
                return;
            ec.assign(0, ec.category());

            parse_authority_form(in, last, ec);
            if(! ec)
                break;
            if(ec != uri::error::mismatch)
                return;
            ec.assign(0, ec.category());

            parse_asterisk_form(in, last, ec);
            if(! ec)
                break;
            if(ec != uri::error::mismatch)
                return;

            ec = uri::error::invalid;
            return;
        }
        if(in != last)
        {
            // extraneous suffix
            ec = uri::error::invalid;
            return;
        }

        // success
        str_ = {base_, last_, base_};
    }
};

//------------------------------------------------------------------------------

template<std::size_t BufferSize = 4096>
class uribuf : public uri_base
{
    char buf_[BufferSize];

public:
    uribuf()
        : uri_base(buf_, sizeof(buf_))
    {
        // prevent value-init of buf_
    }
};

//------------------------------------------------------------------------------

inline
std::ostream&
operator<<(std::ostream& os, uri_view const& u)
{
    return os << u.str();
}

//------------------------------------------------------------------------------

class uri_test : public unit_test::suite
{
public:
    void
    err(string_view s)
    {
        uribuf<> u;
        error_code ec;
        u.parse_request_target(s, ec);
        BEAST_EXPECT(ec);
    }

    void
    print(uri_view const& u)
    {
        log <<
            "str='" << u.str() << "'";
        if(! u.scheme().empty())
            log <<
                ", scheme=" << u.scheme();
        if(! u.authority().empty())
            log <<
                ", authority=" << u.authority();
        if(! u.userinfo().empty())
            log <<
                ", userinfo=" << u.userinfo();
        if(! u.host().empty())
            log <<
                ", host=" << u.host();
        if(! u.port().empty())
            log <<
                ", port=" << u.port();
        if(! u.path().empty())
            log <<
                ", path=" << u.path();
        if(! u.query().empty())
            log <<
                ", query=" << u.query();
        if(! u.fragment().empty())
            log <<
                ", fragment=" << u.fragment();
        log << std::endl;
    }

    void
    testRequestTarget()
    {
        auto const eq =
            [this](string_view s)
            {
                uribuf<> u;
                error_code ec;
                u.parse_request_target(s, ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    return;
                BEAST_EXPECTS(
                    boost::lexical_cast<std::string>(u) == s,
                    boost::lexical_cast<std::string>(u));
                print(u);
            };

        auto const eqs =
            [this](string_view s0, string_view s)
            {
                uribuf<> u;
                error_code ec;
                u.parse_request_target(s0, ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    return;
                BEAST_EXPECTS(
                    boost::lexical_cast<std::string>(u) == s,
                    boost::lexical_cast<std::string>(u));
                print(u);
            };

        // origin-form
        eq("/");
        eq("/a");
        eq("/a/b");
        eq("/a/b/");
        eq("/?");
        eq("/?a");
        eq("/a?b");
        eq("/?");
        eq("//?");
        eq("/?/");
        eq("//?/");
        eqs("/a/%61", "/a/a");
        eqs("/b/%ff", "/b/%FF");

        // absolute-form
        //eq("http://a.b.c");

        // authority-form
        eq("127.0.0.1");
        eq("www.example.com");
        eq("www.example.com:80");

        // asterisk-form
        eq("*");
        err("**");
        err("*/");
        err("*?");
        err("*#");

        // invalid
        err("");        
    }

    void
    run() override
    {
        testRequestTarget();
    }
};

BEAST_DEFINE_TESTSUITE(uri,http,beast);

} // http
} // beast
