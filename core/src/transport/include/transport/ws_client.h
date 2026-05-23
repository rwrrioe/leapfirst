#pragma once
#include <string>
#include <functional>

namespace transport {

	class WebSocketClient {
	public:
		WebSocketClient();
		~WebSocketClient();
		
		bool connect(const std::string& url);
		void disconnect();

		void on_message();
	private:

};
}