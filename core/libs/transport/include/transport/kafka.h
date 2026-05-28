#include <memory>
#include <signal/signal.h>
#include <queue/spsq.h>

class KafkaProducer {
public:
	KafkaProducer(SPSQ<Signal>& ewma,
		SPSQ<Signal>& zscore,
		SPSQ<Signal>& beta,
		SPSQ<Signal>& corr);

	~KafkaProducer();
	
	void run();
	void stop();
private:
	struct Impl;
	std::shared_ptr<Impl> impl_;
};

