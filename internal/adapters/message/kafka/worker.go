package kafkaconsumer

import (
	"log/slog"

	"github.com/twmb/franz-go/pkg/kgo"
)

type kafkaWorker struct {
	client    *kgo.Client
	topic     string
	partition int32

	quit chan struct{}
	done chan struct{}
	recs chan kgo.FetchTopicPartition

	logger *slog.Logger
}

func newKafkaWorker(
	client *kgo.Client,
	topic string,
	partition int32,
	//how many batches we store before processing
	FetchQueueDepth int,
) *kafkaWorker {
	return &kafkaWorker{
		client:    client,
		topic:     topic,
		partition: partition,

		quit: make(chan struct{}),
		done: make(chan struct{}),
		recs: make(chan kgo.FetchTopicPartition, FetchQueueDepth),
	}
}

func (k *kafkaWorker) consume() {
	defer close(k.done)

	k.logger.Info(
		"kafka worker started",
		slog.String("topic", k.topic),
		slog.Int("partition", int(k.partition)),
	)

	for {
		select {
		case <-k.quit:
			return
		case p := <-k.recs:
			//todo add work
			k.client.MarkCommitRecords(p.Records...)
		}
	}

}
