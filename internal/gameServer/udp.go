package gameserver

import (
	"fmt"
	"net"
)

func (g *GameServer) CreateUDPServer() int {
	var err error
	g.UdpListener, err = net.ListenPacket("udp", fmt.Sprintf(":%d", g.Port))
	if err != nil {
		g.Logger.Error(err, "failed to create UDP server")
		return 0
	}
	defer g.UdpListener.Close()
	g.Logger.Info("Created UDP server", "port", g.Port)
	return g.Port
}
