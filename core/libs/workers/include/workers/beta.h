#pragma once
#include <memory>
#include <atomic>
#include <queue/spsq.h>
#include <signal/tick.h>
#include <signal/signal.h>

class BetaWorker {
public:
	BetaWorker(SPSQ<AggTrade>& in,
			   SPSQ<Signal>& out);
	~BetaWorker();

	BetaWorker(const BetaWorker&) = delete;
	BetaWorker& operator=(const BetaWorker) = delete;

	void run();
	void stop();
private:
	struct Impl;
	std::shared_ptr<Impl> impl_;
};