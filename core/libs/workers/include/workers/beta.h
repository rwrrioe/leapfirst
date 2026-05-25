#pragma once
#include <memory>

class BetaWorker {
private:
	struct Impl;
	std::shared_ptr<Impl> impl_;
};