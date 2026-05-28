#pragma once
#include <memory>
#include <queue/spsq.h>
#include <signal/tick.h>
#include <signal/signal.h>

class EwmaWorker {
public:
	explicit EwmaWorker(SPSQ<AggTrade>& in,
		SPSQ<Signal>& out);
	~EwmaWorker();

	EwmaWorker(const EwmaWorker&) = delete;
	EwmaWorker& operator=(const EwmaWorker) = delete;

	void run();
	void stop();
private:
	struct Impl;
	std::shared_ptr<Impl> impl_;
};
