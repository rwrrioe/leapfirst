#pragma once
#include <memory>

class Engine {
public:
	Engine();
	~Engine();
	void run();
	void stop();
private:
	struct Impl;
	std::shared_ptr<Impl> impl_;
};

