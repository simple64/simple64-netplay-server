package lobbyserver

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"time"

	"github.com/go-logr/logr"
	retryablehttp "github.com/hashicorp/go-retryablehttp"
	gameserver "github.com/simple64/simple64-netplay-server/internal/gameServer"
	"golang.org/x/net/websocket"
)

const (
	BadPassword     = 1
	MismatchVersion = 2
	RoomFull        = 3
	DuplicateName   = 4
)

const (
	TypeMessage        = "message"
	TypeRoomPlayes     = "room_players"
	TypeSendRoom       = "send_room"
	TypeSendRoomCreate = "send_room_create"
	TypeAcceptJoin     = "accept_join"
	TypeChatUpdate     = "chat_update"
	TypeBeginGame      = "begin_game"
	TypeSendMotd       = "send_motd"
)

type LobbyServer struct {
	Logger           logr.Logger
	Name             string
	BasePort         int
	DisableBroadcast bool
	GameServers      map[string]*gameserver.GameServer
}

type SocketMessage struct {
	Type           string `json:"type"`
	RoomName       string `json:"room_name"`
	PlayerName     string `json:"player_name"`
	Password       string `json:"password"`
	Message        string `json:"message"`
	MD5            string `json:"MD5"`
	Port           int    `json:"port"`
	GameName       string `json:"game_name"`
	ClientSha      string `json:"client_sha"`
	NetplayVersion int    `json:"netplay_version"`
	LLE            string `json:"lle"`
	IP             string `json:"IP"`
	Protected      string `json:"protected"`
	Accept         int    `json:"accept"`
	Player0        string `json:"0"`
	Player1        string `json:"1"`
	Player2        string `json:"2"`
	Player3        string `json:"3"`
}

const NetplayAPIVersion = 11

func (s *LobbyServer) sendData(ws *websocket.Conn, message SocketMessage) error {
	binaryData, err := json.Marshal(message)
	if err != nil {
		return fmt.Errorf("error marshalling data: %s", err.Error())
	}
	// s.Logger.Info("sending message", "message", message, "IP", ws.Request().RemoteAddr)
	err = websocket.Message.Send(ws, binaryData)
	if err != nil {
		return fmt.Errorf("error sending data: %s", err.Error())
	}
	return nil
}

// this function finds the GameServer pointer based on the port number.
func (s *LobbyServer) findGameServer(port int) (string, *gameserver.GameServer) {
	for i, v := range s.GameServers {
		if v.Port == port {
			return i, v
		}
	}
	return "", nil
}

func (s *LobbyServer) updatePlayers(g *gameserver.GameServer) {
	if g == nil {
		return
	}
	var sendMessage SocketMessage
	sendMessage.Type = TypeRoomPlayes
	for i, v := range g.Players {
		if v.Number == 0 {
			sendMessage.Player0 = i
		} else if v.Number == 1 {
			sendMessage.Player1 = i
		} else if v.Number == 2 { //nolint:gomnd
			sendMessage.Player2 = i
		} else if v.Number == 3 { //nolint:gomnd
			sendMessage.Player3 = i
		}
	}

	// send the updated player list to all connected players
	for _, v := range g.Players {
		if err := s.sendData(v.Socket, sendMessage); err != nil {
			s.Logger.Error(err, "failed to send message", "message", sendMessage)
		}
	}
}

func (s *LobbyServer) publishDiscord(message string, channel string) {
	body := map[string]string{
		"content": message,
	}
	bodyJSON, err := json.Marshal(body)
	if err != nil {
		s.Logger.Error(err, "could not read body")
		return
	}
	httpClient := retryablehttp.NewClient()
	httpClient.Logger = nil
	httpRequest, err := retryablehttp.NewRequest(http.MethodPost, channel, bodyJSON)
	if err != nil {
		s.Logger.Error(err, "could not create request")
	}
	httpRequest.Header.Set("Content-Type", "application/json")
	httpRequest.Header.Set("User-Agent", "simple64Bot (simple64.github.io, 1)")
	resp, err := httpClient.Do(httpRequest)
	if err != nil {
		s.Logger.Error(err, "could not send request")
	} else {
		resp.Body.Close()
	}
}

func (s *LobbyServer) announceDiscord(g *gameserver.GameServer) {
	roomType := "public"
	if g.Password != "" {
		roomType = "private"
	}

	message := fmt.Sprintf("New %s netplay room running in %s has been created! Come play %s", roomType, s.Name, g.GameName)
	for i := 0; i < 10; i++ {
		channel := os.Getenv(fmt.Sprintf("SIMPLE64_CHANNEL_%d", i))
		if channel != "" {
			s.publishDiscord(message, channel)
		}
	}

	devChannel := os.Getenv("SIMPLE64_DEV_CHANNEL")
	if devChannel != "" {
		s.publishDiscord(message, devChannel)
	}
}

func (s *LobbyServer) watchGameServer(name string, g *gameserver.GameServer) {
	go g.ManageBuffer()
	go g.ManagePlayers()
	for {
		if !g.Running {
			s.Logger.Info("game server deleted", "room", name, "port", g.Port)
			delete(s.GameServers, name)
			return
		}
		time.Sleep(time.Second * 5) //nolint:gomnd
	}
}

func (s *LobbyServer) wsHandler(ws *websocket.Conn) {
	defer ws.Close()

	s.Logger.Info("new WS connection", "address", ws.Request().RemoteAddr)

	for {
		var receivedMessage SocketMessage
		err := websocket.JSON.Receive(ws, &receivedMessage)
		if err != nil {
			if errors.Is(err, io.EOF) {
				for i, v := range s.GameServers {
					if !v.Running {
						for k, w := range v.Players {
							if w.Socket == ws {
								s.Logger.Info("Player has left", "player", k, "room", i)
								delete(v.Players, k)
								s.updatePlayers(v)
							}
						}
						if len(v.Players) == 0 {
							s.Logger.Info("No more players in room, deleting", "room", i)
							v.CloseServers()
							delete(s.GameServers, i)
						}
					}
				}
				s.Logger.Info("closed WS connection", "address", ws.Request().RemoteAddr)
				return
			}
			s.Logger.Error(err, "could not read message", "address", ws.Request().RemoteAddr)
			continue
		}

		// s.Logger.Info("received message", "message", receivedMessage)

		var sendMessage SocketMessage

		if receivedMessage.Type == "create_room" {
			_, exists := s.GameServers[receivedMessage.RoomName]
			if exists {
				sendMessage.Type = TypeMessage
				sendMessage.Message = "Room with this name already exists"
				if err := s.sendData(ws, sendMessage); err != nil {
					s.Logger.Error(err, "failed to send message", "message", sendMessage)
				}
			} else if receivedMessage.NetplayVersion != NetplayAPIVersion {
				sendMessage.Type = TypeMessage
				sendMessage.Message = "client and server not at same version. Visit <a href=\"https://simple64.github.io\">here</a> to update"
				if err := s.sendData(ws, sendMessage); err != nil {
					s.Logger.Error(err, "failed to send message", "message", sendMessage)
				}
			} else {
				g := gameserver.GameServer{}
				sendMessage.Port = g.CreateNetworkServers(s.BasePort, receivedMessage.RoomName, receivedMessage.GameName, s.Logger)
				if sendMessage.Port == 0 {
					sendMessage.Type = TypeMessage
					sendMessage.Message = "Failed to create room"
					if err := s.sendData(ws, sendMessage); err != nil {
						s.Logger.Error(err, "failed to send message", "message", sendMessage)
					}
				} else {
					g.Password = receivedMessage.Password
					g.GameName = receivedMessage.GameName
					g.MD5 = receivedMessage.MD5
					g.ClientSha = receivedMessage.ClientSha
					g.Password = receivedMessage.Password
					g.Players = make(map[string]gameserver.Client)
					g.Players[receivedMessage.PlayerName] = gameserver.Client{
						IP:     receivedMessage.IP,
						Number: 0,
						Socket: ws,
					}
					s.GameServers[receivedMessage.RoomName] = &g
					s.Logger.Info("Created new room", "room", receivedMessage.RoomName, "port", g.Port, "game", g.GameName, "creator", receivedMessage.PlayerName, "creatorIP", receivedMessage.IP)
					sendMessage.Type = TypeSendRoomCreate
					sendMessage.RoomName = receivedMessage.RoomName
					sendMessage.GameName = g.GameName
					sendMessage.PlayerName = receivedMessage.PlayerName
					if err := s.sendData(ws, sendMessage); err != nil {
						s.Logger.Error(err, "failed to send message", "message", sendMessage)
					}
					s.announceDiscord(&g)
				}
			}
		} else if receivedMessage.Type == "get_rooms" {
			if receivedMessage.NetplayVersion != NetplayAPIVersion {
				sendMessage.Type = TypeMessage
				sendMessage.Message = "client and server not at same version. Visit <a href=\"https://simple64.github.io\">here</a> to update"
				if err := s.sendData(ws, sendMessage); err != nil {
					s.Logger.Error(err, "failed to send message", "message", sendMessage)
				}
			} else {
				sendMessage.Type = TypeSendRoom
				for i, v := range s.GameServers {
					if v.Running {
						continue
					}
					if v.Password == "" {
						sendMessage.Protected = "No"
					} else {
						sendMessage.Protected = "Yes"
					}
					sendMessage.RoomName = i
					sendMessage.MD5 = v.MD5
					sendMessage.Port = v.Port
					sendMessage.GameName = v.GameName
					if err := s.sendData(ws, sendMessage); err != nil {
						s.Logger.Error(err, "failed to send message", "message", sendMessage)
					}
				}
			}
		} else if receivedMessage.Type == "join_room" {
			var duplicateName bool
			var accepted int
			sendMessage.Type = TypeAcceptJoin
			roomName, g := s.findGameServer(receivedMessage.Port)
			if g != nil {
				for i := range g.Players {
					if receivedMessage.PlayerName == i {
						duplicateName = true
					}
				}
				if g.Password != "" && g.Password != receivedMessage.Password {
					accepted = BadPassword
				} else if g.ClientSha != receivedMessage.ClientSha {
					accepted = MismatchVersion
				} else if len(g.Players) >= 4 { //nolint:gomnd
					accepted = RoomFull
				} else if duplicateName {
					accepted = DuplicateName
				} else {
					var number int
					for number = 0; number < 4; number++ {
						goodNumber := true
						for _, v := range g.Players {
							if v.Number == number {
								goodNumber = false
							}
						}
						if goodNumber {
							break
						}
					}
					g.Players[receivedMessage.PlayerName] = gameserver.Client{
						IP:     receivedMessage.IP,
						Socket: ws,
						Number: number,
					}
					s.Logger.Info("new player joining room", "player", receivedMessage.PlayerName, "playerIP", receivedMessage.IP, "room", roomName, "number", number)
				}
			} else {
				s.Logger.Error(fmt.Errorf("could not find game server"), "server not found", "message", receivedMessage)
			}
			sendMessage.PlayerName = receivedMessage.PlayerName
			sendMessage.Accept = accepted
			sendMessage.RoomName = roomName
			sendMessage.GameName = g.GameName
			sendMessage.Port = g.Port
			if err := s.sendData(ws, sendMessage); err != nil {
				s.Logger.Error(err, "failed to send message", "message", sendMessage)
			}
		} else if receivedMessage.Type == "request_players" {
			_, g := s.findGameServer(receivedMessage.Port)
			if g != nil {
				s.updatePlayers(g)
			} else {
				s.Logger.Error(fmt.Errorf("could not find game server"), "server not found", "message", receivedMessage)
			}
		} else if receivedMessage.Type == "chat_message" {
			sendMessage.Type = TypeChatUpdate
			sendMessage.Message = fmt.Sprintf("%s: %s", receivedMessage.PlayerName, receivedMessage.Message)
			_, g := s.findGameServer(receivedMessage.Port)
			if g != nil {
				for _, v := range g.Players {
					if err := s.sendData(v.Socket, sendMessage); err != nil {
						s.Logger.Error(err, "failed to send message", "message", sendMessage)
					}
				}
			} else {
				s.Logger.Error(fmt.Errorf("could not find game server"), "server not found", "message", receivedMessage)
			}
		} else if receivedMessage.Type == "start_game" {
			sendMessage.Type = TypeBeginGame
			roomName, g := s.findGameServer(receivedMessage.Port)
			if g != nil {
				g.Running = true
				g.StartTime = time.Now()
				g.Logger.Info("starting game", "time", g.StartTime.String())
				go s.watchGameServer(roomName, g)
				sendMessage.Port = g.Port
				for _, v := range g.Players {
					if err := s.sendData(v.Socket, sendMessage); err != nil {
						s.Logger.Error(err, "failed to send message", "message", sendMessage)
					}
				}
			} else {
				s.Logger.Error(fmt.Errorf("could not find game server"), "server not found", "message", receivedMessage)
			}
		} else if receivedMessage.Type == "get_motd" {
			sendMessage.Type = TypeSendMotd
			sendMessage.Message = "Join <a href=\"https://discord.gg/tsR3RtYynZ\">The Discord Server</a> to find more players!"
			if err := s.sendData(ws, sendMessage); err != nil {
				s.Logger.Error(err, "failed to send message", "message", sendMessage)
			}
		} else {
			s.Logger.Error(fmt.Errorf("invalid type"), "not a valid type", "message", receivedMessage)
		}
	}
}

// this function figures out what is our outgoing IP address.
func (s *LobbyServer) getOutboundIP(dest *net.UDPAddr) (net.IP, error) {
	conn, err := net.DialUDP("udp", nil, dest)
	if err != nil {
		return nil, fmt.Errorf("error creating udp %s", err.Error())
	}
	defer conn.Close()
	localAddr, ok := conn.LocalAddr().(*net.UDPAddr)
	if !ok {
		return nil, fmt.Errorf("failed to parse address")
	}

	return localAddr.IP, nil
}

func (s *LobbyServer) processBroadcast(udpServer *net.UDPConn, addr *net.UDPAddr, buf []byte) {
	if buf[0] == 1 {
		s.Logger.Info(fmt.Sprintf("received broadcast from %s on %s", addr.String(), udpServer.LocalAddr().String()))
		// send back the address of the WebSocket server
		outboundIP, err := s.getOutboundIP(addr)
		if err != nil {
			s.Logger.Error(err, "could not get outbound IP")
			return
		}
		response := map[string]string{
			s.Name: fmt.Sprintf("ws://%s", net.JoinHostPort(outboundIP.String(), fmt.Sprint(s.BasePort))),
		}
		jsonData, err := json.Marshal(response)
		if err != nil {
			s.Logger.Error(err, "could not encode json data")
			return
		}
		_, err = udpServer.WriteTo(jsonData, addr)
		if err != nil {
			s.Logger.Error(err, "could not reply to broadcast")
			return
		}
		s.Logger.Info("responded to broadcast", "response", response)
	}
}

func (s *LobbyServer) runBroadcastServer(broadcastPort int) {
	broadcastServer, err := net.ListenUDP("udp", &net.UDPAddr{Port: broadcastPort})
	if err != nil {
		s.Logger.Error(err, "could not listen for broadcasts")
		return
	}
	defer broadcastServer.Close()

	s.Logger.Info("listening for broadcasts")
	for {
		buf := make([]byte, 1024) //nolint:gomnd
		_, addr, err := broadcastServer.ReadFromUDP(buf)
		if err != nil {
			s.Logger.Error(err, "error reading broadcast packet")
			continue
		}
		s.processBroadcast(broadcastServer, addr, buf)
	}
}

func (s *LobbyServer) RunSocketServer(broadcastPort int) error {
	s.GameServers = make(map[string]*gameserver.GameServer)
	if !s.DisableBroadcast {
		go s.runBroadcastServer(broadcastPort)
	}

	server := websocket.Server{
		Handler:   s.wsHandler,
		Handshake: nil,
	}
	http.Handle("/", server)
	listenAddress := fmt.Sprintf(":%d", s.BasePort)
	s.Logger.Info("server running", "address", listenAddress)
	err := http.ListenAndServe(listenAddress, nil) //nolint:gosec
	if err != nil {
		return fmt.Errorf("error listening on http port %s", err.Error())
	}
	return nil
}
