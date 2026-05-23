#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "signal/tick.h"

class WebSocketClient {
public:
	struct Config {
		std::string host;
		std::string port;
		std::vector<std::string> streams;
		int reconnect_delay_ms;
		int ping_interval_ms;
	};

	explicit WebSocketClient(Config cfg);
	~WebSocketClient();

	WebSocketClient(const WebSocketClient&) = delete;
	WebSocketClient& operator=(const WebSocketClient&) = delete;

	void run();
	void stop();
private:
	struct Impl;
	std::shared_ptr<Impl> impl_;
};