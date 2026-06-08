package ports

import (
	"context"

	"github.com/rwrrioe/leapfirst/internal/domain"
)

type Storage interface {
	SaveTick()
	GetTick()
	BatchSaveTick(ctx context.Context, signals []domain.Signal) error
}
