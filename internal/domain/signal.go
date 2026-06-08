package domain

type SignalType int

const (
	EWMA_CROSS SignalType = iota
	ZSCORE
	BETA
	CORRELATION
)

type Direction int

const (
	BUY Direction = iota
	SELL
)

type SignalSymbol int

const (
	BTCUSDT SignalSymbol = iota
	ETHUSDT
)

type Signal struct {
	Timestamp    int
	Value        float64
	SignalType   SignalType
	Direction    Direction
	SignalSymbol SignalSymbol
}
