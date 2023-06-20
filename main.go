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

func main() {
	zapLog, err := zap.NewProduction()
	if err != nil {
		log.Panic(err)
	}
	logger := zapr.NewLogger(zapLog)

	name := flag.String("name", "local-server", "Server name")
	basePort := flag.Int("baseport", DefaultBasePort, "Base port")
	disableBroadcast := flag.Bool("disable-broadcast", false, "Disable LAN broadcast")
	flag.Parse()
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
