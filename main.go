package main

import (
	"flag"
	"fmt"
	"log"
	"os"

	"github.com/go-logr/zapr"
	socketserver "github.com/simple64/simple64-netplay-server/internal/socketServer"
	"go.uber.org/zap"
)

func main() {
	zapLog, err := zap.NewProduction()
	if err != nil {
		log.Panic(err)
	}
	logger := zapr.NewLogger(zapLog)

	name := flag.String("name", "", "Server name (required)")
	basePort := flag.Int("baseport", 45000, "Base port")
	disableBroadcast := flag.Bool("disable-broadcast", false, "Disable LAN broadcast")
	flag.Parse()
	if *name == "" {
		logger.Error(fmt.Errorf("name required"), "server name not set")
		os.Exit(1)
	}

	s := socketserver.SocketServer{
		Logger:           logger,
		Name:             *name,
		BasePort:         *basePort,
		DisableBroadcast: *disableBroadcast,
	}
	if err := s.RunSocketServer(); err != nil {
		logger.Error(err, "could not run socket server")
	}
}
