package ports

import (
	"context"

	"github.com/rwrrioe/leapfirst/internal/domain"
)

type MessageProvider interface {
	Poll(ctx context.Context) ([]domain.Signal, error)
}
