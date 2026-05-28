#pragma once
#include <memory>
#include <queue/spsq.h>
#include <signal/tick.h>
#include <signal/signal.h>

class CorrelationWorker {
public:
	CorrelationWorker(SPSQ<AggTrade>& in,
					 SPSQ<Signal>& out);
	~CorrelationWorker();


	CorrelationWorker(const CorrelationWorker&) = delete;
	CorrelationWorker& operator=(const CorrelationWorker) = delete;

	void run();
	void stop();
private:
	struct Impl;
	std::shared_ptr<Impl> impl_;
};