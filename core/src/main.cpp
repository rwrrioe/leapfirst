#include <iostream>
#include <signal/engine.h>
#include <csignal>
static Engine* g_engine = nullptr;

void handle_signal(int) {
	if (g_engine) g_engine->stop();
}

int main() {
	
	std::signal(SIGINT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	try {
		Engine engine = Engine();
		engine.run();
	}
	catch (const std::exception& e) {
		std::printf("main: %s\n", e.what());
		fflush(stdout);
	}
	catch (...) {
		std::printf("main unknown exception:n");
		fflush(stdout);
	}
}