package postgresql

import (
	"context"
	"fmt"
	"log/slog"
	"os"

	"github.com/jackc/pgx/v5/pgxpool"
)

func dsn() string {
	return fmt.Sprintf(
		"postgres://%s:%s@%s:%d/%s?sslmode=%s",
		os.Getenv("DB_USER"),
		os.Getenv("DB_PASSWD"),
		os.Getenv("DB_HOST"),
		os.Getenv("DB_PORT"),
		os.Getenv("DB_NAME"),
		os.Getenv("DB_SSL_MODE"),
	)
}

func New(ctx context.Context, logger *slog.Logger) (*pgxpool.Pool, error) {
	const op = "postgresql.New"

	log := logger.With(
		slog.String("op", op),
	)

	dsn := dsn()
	pool, err := pgxpool.New(ctx, dsn)
	if err != nil {
		log.Error("failed to create pgxpool")
		return nil, fmt.Errorf("%s:%w", op, err)
	}

	return pool, nil
}
