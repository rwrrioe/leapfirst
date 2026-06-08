package usecase

import (
	"context"
	"fmt"
	"log/slog"

	"github.com/rwrrioe/leapfirst/internal/ports"
)

type Processor struct {
	MessageProvider ports.MessageProvider
	Storage         ports.Storage
	logger          *slog.Logger
}

func (p *Processor) ProcessTickAndSave(ctx context.Context) error {
	const op = "usecase.Processor.ProcessTickAndSave"

	log := p.logger.With(
		slog.String("op", op),
	)

	signals, err := p.MessageProvider.Poll(ctx)
	if err != nil {
		return fmt.Errorf("%s:%w", op, err)
	}

	if err := p.Storage.BatchSaveTick(ctx, signals); err != nil {
		return fmt.Errorf("%s:%w", op, err)
	}

	lastTimestamp := signals[len(signals)-1].Timestamp
	log.Info(
		"batch successfully ssaved",
		slog.Int("last timestamp", lastTimestamp),
	)
	return nil
}
