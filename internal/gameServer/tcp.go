package gameserver

import (
	"net"
)

const MAX_GAMES = 20

func (g *GameServer) createTCPServer(basePort int) int {
	var err error
	for i := 0; i < MAX_GAMES; i++ {
		g.TcpListener, err = net.ListenTCP("tcp", &net.TCPAddr{Port: basePort + i})
		if err == nil {
			defer g.TcpListener.Close()
			g.Port = basePort + i
			g.Logger.Info("Created TCP server", "port", g.Port)
			return g.Port
		}
	}
	return 0
}
