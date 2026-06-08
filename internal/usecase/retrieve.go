package usecase

import (
	"log/slog"
	"time"

	"github.com/rwrrioe/leapfirst/internal/ports"
)

type Config struct{}

type Retriever struct {
	Storage ports.Storage
	logger  *slog.Logger
}

func (r *Retriever) Retrieve(date time.Time) {

}
