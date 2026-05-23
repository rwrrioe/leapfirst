#include <transport/include/transport/websocket_client.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <charconv>
#include <chrono>
#include <sstream>
#include <string>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = asio::ssl;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;
using json = nlohmann::json;


struct WebSocketClient::Impl : public std::enable_shared_from_this<Impl> {
public:
private:
	asio::io_context io_ctx;
	ssl::context ssl_ctx;
	tcp::resolver resolver_;
	websocket::stream<ssl::stream<beast::tcp_stream>> ws_;
	beast::flat_buffer buffer_;
	Config cfg;
	


	explicit Impl(Config config)
		: cfg(std::move(config))
		, ssl_ctx(ssl::context::tlsv12_client)
		, resolver_(io_ctx)
		, ws_(io_ctx, ssl_ctx)
	{}

	void run() {
		ws_.next_layer().set_verify_callback(ssl::host_name_verification(cfg.host));

		resolver_.async_resolve(
			cfg.host,
			cfg.port,
			beast::bind_front_handler(
				&Impl::on_resolve,
				shared_from_this()));
	}

	void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
		if (ec)
			return;

		beast::get_lowest_layer(ws_).async_connect(
			results,
			beast::bind_front_handler(
				&Impl::on_connect,
				shared_from_this()));
	}

	void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
		if (ec)
			return;

		cfg.host += ":" + std::to_string(ep.port());

		ws_.next_layer().async_handshake(
			ssl::stream_base::client,
			beast::bind_front_handler(
				&Impl::on_ssl_handshake,
				shared_from_this()));
	}

	void on_ssl_handshake(beast::error_code ec) {
		if (ec)
			return;

		beast::get_lowest_layer(ws_).expires_never();

		ws_.set_option(
			websocket::stream_base::timeout::suggested(
			beast::role_type::client));
		
		ws_.set_option(websocket::stream_base::decorator(
			[](websocket::request_type& req)
			{
				req.set(http::field::user_agent,
					std::string(BOOST_BEAST_VERSION_STRING) +
					" websocket-client-async-ssl");
			}));
		

		ws_.async_handshake(cfg.host, "/",
			beast::bind_front_handler(
				&Impl::on_handshake,
				shared_from_this()));
	}

	void on_handshake(beast::error_code ec) {
		if (ec)
			return;

		ws_.async_read(
			buffer_,
			beast::bind_front_handler(
				&Impl::on_read,
				shared_from_this()));
	}

	void on_read(beast::error_code ec, std::size_t bytes_transferred)
	{
		if (ec)
			//todo reconnect 
			return;

		const auto* data = static_cast<const char*>(buffer_.data().data());
		const auto size = buffer_.size();

		// call dispatch here

		buffer_.consume(buffer_.size());
		ws_.async_read(
			buffer_,
			beast::bind_front_handler(
				&Impl::on_read,
				shared_from_this()));
	}

};

WebSocketClient::WebSocketClient(Config cfg, )