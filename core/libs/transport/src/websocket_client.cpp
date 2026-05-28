#include <transport/websocket_client.h>
#include <transport/types.h>

#include <openssl/x509.h>
#include <openssl/ssl.h>
#ifdef _WIN32
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#endif

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <simdjson.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <sstream>
#include <string>
#include <string_view>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = asio::ssl;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;
using json = simdjson::ondemand::parser;
using json_object = simdjson::ondemand::object;

struct WebSocketClient::Impl : public std::enable_shared_from_this<Impl> {
public:
	explicit Impl(Config config, DispatchCallback cb)
		: cfg(std::move(config))
		, on_tick_(std::move(cb))
		, ssl_ctx(ssl::context::tls_client)
		, resolver_(io_ctx)
		, ws_(io_ctx, ssl_ctx)
	{
	}

	void run() {
		ssl_ctx.set_verify_mode(ssl::verify_none);

		if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), cfg.host.c_str())) {
			std::printf("[WS] SNI failed\n"); fflush(stdout);
		}

		resolver_.async_resolve(
			cfg.host,
			cfg.port,
			beast::bind_front_handler(
				&Impl::on_resolve,
				shared_from_this()));

		io_ctx.run();
	}

	void stop() {
		asio::post(io_ctx, [this] {
			beast::error_code ec;
			ws_.async_close(websocket::close_code::normal,
				[](beast::error_code) {});
			});
	}

private:
	asio::io_context io_ctx;
	ssl::context ssl_ctx;
	tcp::resolver resolver_;
	websocket::stream<ssl::stream<beast::tcp_stream>> ws_;
	beast::flat_buffer buffer_;
	Config cfg;
	json parser_;
	
	std::string host_header_;
	DispatchCallback on_tick_;

	//2^16, given that the max package <= 65kB
	static const int SIMDJSON_PADDING{ 64 };
	alignas(64) char pad_[65536 + SIMDJSON_PADDING];
	void on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
		if (ec)
		{
			std::printf("[WS] resolve failed: %s\n", ec.message().c_str());
			return;
		}
		std::printf("[WS] resolved ok\n");

		beast::get_lowest_layer(ws_).async_connect(
			results,
			beast::bind_front_handler(
				&Impl::on_connect,
				shared_from_this()));
	}

	void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
		if (ec) { std::printf("[WS] connect failed: %s\n", ec.message().c_str()); fflush(stdout); return; }
		std::printf("[WS] connected ok\n"); fflush(stdout);

		host_header_ = cfg.host + ":" + std::to_string(ep.port());
		std::printf("[WS] host_header: %s\n", host_header_.c_str()); fflush(stdout);

		std::printf("[WS] starting ssl handshake...\n"); fflush(stdout);
		
		try {
			ws_.next_layer().async_handshake(
				ssl::stream_base::client,
				beast::bind_front_handler(&Impl::on_ssl_handshake, shared_from_this()));
			std::printf("[WS] ssl handshake posted\n"); fflush(stdout);
		}
		catch (const std::exception& e) {
			std::printf("[WS] async_handshake threw: %s\n", e.what()); fflush(stdout);
		}
		catch (...) {
			std::printf("[WS] async_handshake threw unknown\n"); fflush(stdout);
		}
	}

	void on_ssl_handshake(beast::error_code ec) {
		if (ec) { std::printf("[WS] ssl handshake failed: %s\n", ec.message().c_str()); return; }
		std::printf("[WS] ssl ok\n");

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


		ws_.async_handshake(host_header_, build_path(cfg.streams),
			beast::bind_front_handler(
				&Impl::on_handshake,
				shared_from_this()));
	}

	void on_handshake(beast::error_code ec) {
		if (ec) { std::printf("[WS] ws handshake failed: %s\n", ec.message().c_str()); return; }
		std::printf("[WS] ws handshake ok, listening...\n");

		ws_.async_read(
			buffer_,
			beast::bind_front_handler(
				&Impl::on_read,
				shared_from_this()));
	}

	void on_read(beast::error_code ec, std::size_t bytes_transferred)
	{
		if (ec) { std::printf("[WS] read failed: %s\n", ec.message().c_str()); return; }
		//std::printf("[WS] got %zu bytes\n", bytes_transferred);

		const auto* data = static_cast<const char*>(buffer_.data().data());
		const auto size = buffer_.size();

		dispatch({data, size});

		buffer_.consume(buffer_.size());
		ws_.async_read(
			buffer_,
			beast::bind_front_handler(
				&Impl::on_read,
				shared_from_this()));
	}


	void dispatch(std::string_view msg) {
		std::memcpy(pad_, msg.data(), msg.size());

		auto doc = parser_.iterate(
			pad_,
			msg.size(),
			sizeof(pad_)
		);

		std::string_view stream_name;
		if (doc["stream"].get(stream_name) != simdjson::SUCCESS) {
			return;
		}

		json_object data;
		if (doc["data"].get(data) != simdjson::SUCCESS) {
			return;
		}
		

		on_tick_(stream_name, data);
	}

	static std::string build_path(const std::vector<std::string>& streams) {
		std::string path = "/stream?streams=";
		for (std::size_t i = 0; i < streams.size(); ++i) {
			if (i > 0) path += '/';
			path += streams[i];
		}
		return path;
	}
};

WebSocketClient::WebSocketClient (Config cfg, DispatchCallback cb)
	: impl_(std::make_shared<Impl>(std::move(cfg), std::move(cb)))
{}


void WebSocketClient::run() { impl_->run(); }
void WebSocketClient::stop() { impl_->stop(); }

WebSocketClient::~WebSocketClient() { impl_->stop(); }


