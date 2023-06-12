package gameserver

import (
	"encoding/binary"
	"fmt"
	"math"
	"net"
	"time"

	"github.com/cespare/xxhash"
)

type GameData struct {
	PlayerAddresses []*net.UDPAddr
	LeadCount       []uint32
	Status          byte
	BufferSize      []uint32
	BufferHealth    []int32
	Inputs          []map[uint32]uint32
	Plugin          []map[uint32]byte
	PendingInput    []uint32
	PendingPlugin   []byte
	SyncHash        map[uint32]uint64
	PlayerAlive     []bool
}

const (
	KEY_INFO_CLIENT      = 0
	KEY_INFO_SERVER      = 1
	PLAYER_INPUT_REQUEST = 2
	CP0_INFO             = 4
)

const STATUS_DESYNC = 1
const DISCONNECT_TIMEOUT_S = 40
const INPUT_DATA_MAX uint32 = 5000

func (g *GameServer) fillInput(playerNumber byte, count uint32) {
	_, inputExists := g.GameData.Inputs[playerNumber][count]
	delete(g.GameData.Inputs[playerNumber], count-INPUT_DATA_MAX) // no need to keep old input data
	delete(g.GameData.Plugin[playerNumber], count-INPUT_DATA_MAX) // no need to keep old input data
	if !inputExists {
		g.GameData.Inputs[playerNumber][count] = g.GameData.PendingInput[playerNumber]
		g.GameData.Plugin[playerNumber][count] = g.GameData.PendingPlugin[playerNumber]
	}
}

func (g *GameServer) sendUDPInput(count uint32, addr *net.UDPAddr, playerNumber byte, spectator bool) {
	buffer := make([]byte, 512)
	countLag := g.GameData.LeadCount[playerNumber] - count
	buffer[0] = KEY_INFO_SERVER
	buffer[1] = playerNumber
	buffer[2] = g.GameData.Status
	buffer[3] = uint8(countLag)
	currentByte := 5
	start := count
	end := start + g.GameData.BufferSize[playerNumber]
	_, ok := g.GameData.Inputs[playerNumber][count] // check if input exists for this count
	for (currentByte < 500) && ((!spectator && countLag == 0 && (count-end) > (math.MaxUint32/2)) || ok) {
		binary.BigEndian.PutUint32(buffer[currentByte:], count)
		currentByte += 4
		g.fillInput(playerNumber, count)
		binary.BigEndian.PutUint32(buffer[currentByte:], g.GameData.Inputs[playerNumber][count])
		currentByte += 4
		buffer[currentByte] = g.GameData.Plugin[playerNumber][count]
		currentByte += 1
		count += 1
		_, ok = g.GameData.Inputs[playerNumber][count] // check if input exists for this count
	}
	buffer[4] = uint8(count - start) //number of counts in packet
	if currentByte > 5 {
		_, err := g.UdpListener.WriteToUDP(buffer[0:currentByte], addr)
		if err != nil {
			g.Logger.Error(err, "could not send input")
		}
	}
}

func (g *GameServer) processUDP(addr *net.UDPAddr, buf []byte) {
	playerNumber := buf[1]
	if buf[0] == KEY_INFO_CLIENT {
		g.GameData.PlayerAddresses[playerNumber] = addr
		count := binary.BigEndian.Uint32(buf[2:])

		g.GameData.PendingInput[playerNumber] = binary.BigEndian.Uint32(buf[6:])
		g.GameData.PendingPlugin[playerNumber] = buf[10]

		for i := 0; i < 4; i++ {
			if g.GameData.PlayerAddresses[i] != nil {
				g.sendUDPInput(count, g.GameData.PlayerAddresses[i], playerNumber, true)
			}
		}
	} else if buf[0] == PLAYER_INPUT_REQUEST {
		regId := binary.BigEndian.Uint32(buf[2:])
		var i byte
		for i = 0; i < 4; i++ {
			v, ok := g.Registrations[i]
			if ok {
				if v.RegId == regId {
					g.GameData.PlayerAlive[i] = true
				}
			}
		}
		count := binary.BigEndian.Uint32(buf[6:])
		spectator := buf[10]
		if ((count - g.GameData.LeadCount[playerNumber]) < (math.MaxUint32 / 2)) && spectator == 0 {
			g.GameData.BufferHealth[playerNumber] = int32(buf[11])
			g.GameData.LeadCount[playerNumber] = count
		}
		g.sendUDPInput(count, addr, playerNumber, spectator != 0)
	} else if buf[0] == CP0_INFO {
		if g.GameData.Status&1 == 0 {
			viCount := binary.BigEndian.Uint32(buf[1:])
			_, ok := g.GameData.SyncHash[viCount]
			if !ok {
				if len(g.GameData.SyncHash) > 500 { // no need to keep old sync hashes
					g.GameData.SyncHash = make(map[uint32]uint64)
				}
				g.GameData.SyncHash[viCount] = xxhash.Sum64(buf[5:133])
			} else if g.GameData.SyncHash[viCount] != xxhash.Sum64(buf[5:133]) {
				g.GameData.Status |= STATUS_DESYNC
				g.Logger.Error(fmt.Errorf("desync"), "game has desynced")
			}
		}
	}
}

func (g *GameServer) watchUDP() {
	for {
		buf := make([]byte, 1024)
		_, addr, err := g.UdpListener.ReadFromUDP(buf)
		if err != nil && !g.isConnClosed(err) {
			g.Logger.Error(err, "error from UdpListener")
			continue
		} else if g.isConnClosed(err) {
			return
		}
		g.processUDP(addr, buf)
	}
}

func (g *GameServer) ManageBuffer() {
	for {
		if !g.Running {
			g.Logger.Info("done managing buffers")
			return
		}
		for i := 0; i < 4; i++ {
			if g.GameData.BufferHealth[i] != -1 {
				if g.GameData.BufferHealth[i] > BUFFER_TARGET && g.GameData.BufferSize[i] > 0 {
					g.GameData.BufferSize[i] -= 1
					g.Logger.Info("reducing buffer size", "player", i, "bufferSize", g.GameData.BufferSize[i])
				} else if g.GameData.BufferHealth[i] < BUFFER_TARGET {
					g.GameData.BufferSize[i] += 1
					g.Logger.Info("increasing buffer size", "player", i, "bufferSize", g.GameData.BufferSize[i])
				}
			}
		}
		time.Sleep(time.Second * 5)
	}
}

func (g *GameServer) ManagePlayers() {
	for {
		if len(g.GameData.Inputs[0]) == 0 { // wait for game to start
			time.Sleep(time.Second * DISCONNECT_TIMEOUT_S)
			continue
		}
		playersActive := false // used to check if anyone is still around
		var i byte
		for i = 0; i < 4; i++ {
			_, ok := g.Registrations[i]
			if ok {
				if g.GameData.PlayerAlive[i] {
					g.Logger.Info("player alive", "player", i, "regID", g.Registrations[i].RegId)
					playersActive = true
				} else {
					g.Logger.Info("play disconnected UDP", "player", i, "regID", g.Registrations[i].RegId)
					g.GameData.Status |= (0x1 << (i + 1))
					delete(g.Registrations, i)
				}
			}
			g.GameData.PlayerAlive[i] = false
		}
		if !playersActive {
			g.Logger.Info("no more players, closing room", "playTime", time.Since(g.StartTime).String())
			g.closeServers()
			g.Running = false
			return
		}
		time.Sleep(time.Second * DISCONNECT_TIMEOUT_S)
	}
}

func (g *GameServer) createUDPServer() int {
	var err error
	g.UdpListener, err = net.ListenUDP("udp", &net.UDPAddr{Port: g.Port})
	if err != nil {
		g.Logger.Error(err, "failed to create UDP server")
		return 0
	}
	g.Logger.Info("Created UDP server", "port", g.Port)

	g.GameData.PlayerAddresses = make([]*net.UDPAddr, 4)
	g.GameData.LeadCount = make([]uint32, 4)
	g.GameData.BufferSize = []uint32{3, 3, 3, 3}
	g.GameData.BufferHealth = []int32{-1, -1, -1, -1}
	g.GameData.Inputs = make([]map[uint32]uint32, 4)
	for i := 0; i < 4; i++ {
		g.GameData.Inputs[i] = make(map[uint32]uint32)
	}
	g.GameData.Plugin = make([]map[uint32]byte, 4)
	for i := 0; i < 4; i++ {
		g.GameData.Plugin[i] = make(map[uint32]byte)
	}
	g.GameData.PendingInput = make([]uint32, 4)
	g.GameData.PendingPlugin = make([]byte, 4)
	g.GameData.SyncHash = make(map[uint32]uint64)
	g.GameData.PlayerAlive = make([]bool, 4)

	go g.watchUDP()
	return g.Port
}
