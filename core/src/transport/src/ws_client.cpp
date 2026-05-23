#include "transport/ws_client.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

class WebSocketClient : public std::enable_shared_from_this <WebSocketClient> {
public:
	explicit
		WebSocketClient(asio::io_context& ioc, ssl::context& ctx, int on_resolve_timeout)
		: resolver_(asio::make_strand(ioc))
		, ws_(net::make_strand(ioc), ctx) 
	{
	}

	void run(
		char const* host,
		char const* port,
		char const* text) {

		ws_.next_layer().set_verify_callback(ssl::host_name_verification(host));
		host_ = host;
		text_ = text;
		on_resolve_timeout = on_resolve_timeout;

		resolver_.async_resolve(
			host,
			port,
			beast::bind_front_handler(
				&WebSocketClient::on_resolve,
				shared_from_this()));

	}


private:
	tcp::resolver resolver_;
	websocket::stream<ssl::stream<beast::tcp_stream>> ws_;
	beast::flat_buffer buffer_;
	std::string host_;
	std::string target_;
	std::string txt_;
	int on_resolve_timeot_;

	void on_resolve(
		beast::error_code ec,
		tcp::resolver::results_type results) 
	{
		if (ec)
			return fail(ec, "failed to resolve");
	

		beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(on_resolve_timeout));

		beast::get_lowest_layer(ws_).async_connect(
			results,
			beast::bind_front_handler(
				&WebSocketClient::on_connect,
				shared_from_this()));
	}
	
};
  