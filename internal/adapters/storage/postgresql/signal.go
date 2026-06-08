package postgresql

import (
	"context"

	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/rwrrioe/leapfirst/internal/domain"
)

type SignalRepo struct {
	db *pgxpool.Pool
}

func NewSignalRepo(db *pgxpool.Pool) *SignalRepo {
	return &SignalRepo{
		db: db,
	}
}

func BatchSaveTick(ctx context.Context, signals []domain.Signal) error {
	//todo add batch insert
	return nil
}
