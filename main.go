package main

import (
	"flag"
	"fmt"
	"log"
	"os"

	"github.com/go-logr/zapr"
	lobbyserver "github.com/simple64/simple64-netplay-server/internal/lobbyServer"
	"go.uber.org/zap"
)

const DefaultBasePort = 45000

func newZap(logPath string) (*zap.Logger, error) {
	cfg := zap.NewProductionConfig()
	cfg.OutputPaths = []string{"stderr"}
	if logPath != "" {
		cfg.OutputPaths = append(cfg.OutputPaths, logPath)
	}
	return cfg.Build()
}

func main() {
	name := flag.String("name", "local-server", "Server name")
	basePort := flag.Int("baseport", DefaultBasePort, "Base port")
	disableBroadcast := flag.Bool("disable-broadcast", false, "Disable LAN broadcast")
	logPath := flag.String("log-path", "", "Write logs to this file")
	flag.Parse()

	zapLog, err := newZap(*logPath)
	if err != nil {
		log.Panic(err)
	}
	logger := zapr.NewLogger(zapLog)

	if *name == "" {
		logger.Error(fmt.Errorf("name required"), "server name not set")
		os.Exit(1)
	}

	s := lobbyserver.LobbyServer{
		Logger:           logger,
		Name:             *name,
		BasePort:         *basePort,
		DisableBroadcast: *disableBroadcast,
	}
	if err := s.RunSocketServer(DefaultBasePort); err != nil {
		logger.Error(err, "could not run socket server")
	}
}
