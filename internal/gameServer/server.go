package gameserver

import (
	"net"

	"golang.org/x/net/websocket"
)

type Client struct {
	IP     string
	Socket *websocket.Conn
	Number int
}

type GameServer struct {
	Port        int
	TcpListener net.Listener
	Password    string
	Running     bool
	MD5         string
	GameName    string
	Players     map[string]Client
	ClientSha   string
}
