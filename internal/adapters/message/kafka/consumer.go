package kafkaconsumer

import (
	"context"
	"log/slog"

	"github.com/twmb/franz-go/pkg/kgo"
)

type topic string
type partition int32

type tp struct {
	t topic
	p partition
}

type Consumer struct {
	client  *kgo.Client
	workers map[tp]*kafkaWorker

	logger *slog.Logger

	MaxPoll         int
	FetchQueueDepth int
}

func (c *Consumer) assign(
	client *kgo.Client,
	assigned map[topic][]partition,
) {
	for t, ps := range assigned {
		for _, p := range ps {
			worker := newKafkaWorker(client, string(t), int32(p), c.FetchQueueDepth)
			c.workers[tp{t, p}] = worker
			go worker.consume()
		}
	}
}

func (c *Consumer) Poll(ctx context.Context) {
	const op = "kafka.consumer.Poll"

	log := c.logger.With("op", op)

	for {
		fetches := c.client.PollRecords(ctx, c.MaxPoll)
		if fetches.IsClientClosed() {
			log.Error("the client is closed")
			return
		}

		fetches.EachError(func(_ string, _ int32, err error) {
			log.Error("failed to fetch: %w", err)
		})

		fetches.EachPartition(func(p kgo.FetchTopicPartition) {
			tp := tp{topic(p.Topic), partition(p.Partition)}

			c.workers[tp].recs <- p
		})
	}
