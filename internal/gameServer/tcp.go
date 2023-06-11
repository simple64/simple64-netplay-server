package gameserver

import (
	"fmt"
	"net"
)

const MAX_GAMES = 20

func (g *GameServer) CreateTCPServer(basePort int) int {
	var err error
	for i := 0; i < MAX_GAMES; i++ {
		g.TcpListener, err = net.Listen("tcp", fmt.Sprintf(":%d", basePort+i))
		if err == nil {
			defer g.TcpListener.Close()
			g.Port = basePort + i
			g.Logger.Info("Created TCP server", "port", g.Port)
			return g.Port
		}
	}
	return 0
}
