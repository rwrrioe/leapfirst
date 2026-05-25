#pragma once
#include <memory>

class Engine {
public:
	Engine() {}
	void run() {}
private:
	struct Impl;
	std::shared_ptr<Impl> impl_;
};