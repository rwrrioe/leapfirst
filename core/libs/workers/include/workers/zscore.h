#pragma once
#include <memory>
#include <signal/tick.h>
#include <signal/signal.h>
#include <queue/spsq.h>

class ZScoreWorker {
public:
	explicit ZScoreWorker(SPSQ<AggTrade>& in, SPSQ<Signal>& out);
	~ZScoreWorker();

	ZScoreWorker(const ZScoreWorker&) = delete;
	ZScoreWorker& operator=(const ZScoreWorker) = delete;

	void run();
	void stop();
private:
	struct Impl;
	std::shared_ptr<Impl> impl_;
};
