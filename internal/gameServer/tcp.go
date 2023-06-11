package gameserver

import (
	"fmt"
	"net"

	"github.com/go-logr/logr"
)

func (g *GameServer) CreateTCPServer(basePort int, logger logr.Logger) int {
	var err error
	for i := 0; i < 20; i++ {
		g.TcpListener, err = net.Listen("tcp", fmt.Sprintf(":%d", basePort+i))
		if err == nil {
			defer g.TcpListener.Close()
			g.Port = basePort + i
			logger.Info("Created TCP server", "port", g.Port)
			return g.Port
		}
	}
	return 0
}
