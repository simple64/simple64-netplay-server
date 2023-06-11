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
	Inputs          map[byte]map[uint32]uint32
	Plugin          map[byte]map[uint32]byte
	PendingInputs   map[byte][]uint32
	PendingPlugin   map[byte][]byte
	SyncHash        map[uint32]uint64
}

const (
	KEY_INFO_CLIENT      = 0
	KEY_INFO_SERVER      = 1
	PLAYER_INPUT_REQUEST = 2
	CP0_INFO             = 4
)

const STATUS_DESYNC = 1

func (g *GameServer) checkIfExists(playerNumber byte, count uint32) bool {
	_, inputExists := g.GameData.Inputs[playerNumber][count]
	delete(g.GameData.Inputs[playerNumber], count-5000)
	delete(g.GameData.Plugin[playerNumber], count-5000)
	if !inputExists {
		_, hasLastInput := g.GameData.Inputs[playerNumber][count-1]
		if len(g.GameData.PendingInputs[playerNumber]) > 0 {
			// pop the first item from the PendingInputs list
			var pendingInput uint32
			pendingInput, g.GameData.PendingInputs[playerNumber] = g.GameData.PendingInputs[playerNumber][0], g.GameData.PendingInputs[playerNumber][1:]
			g.GameData.Inputs[playerNumber][count] = pendingInput
			// pop the first item from the PendingPlugin list
			var pendingPlugin byte
			pendingPlugin, g.GameData.PendingPlugin[playerNumber] = g.GameData.PendingPlugin[playerNumber][0], g.GameData.PendingPlugin[playerNumber][1:]
			g.GameData.Plugin[playerNumber][count] = pendingPlugin
		} else if hasLastInput {
			// duplicate last input
			g.GameData.Inputs[playerNumber][count] = g.GameData.Inputs[playerNumber][count-1]
			g.GameData.Plugin[playerNumber][count] = g.GameData.Plugin[playerNumber][count-1]
		} else {
			g.GameData.Inputs[playerNumber][count] = 0 // Controller not present
			g.GameData.Plugin[playerNumber][count] = 0 // Controller not present
		}
		return true
	}
	return inputExists
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
		if !g.checkIfExists(playerNumber, count) {
			// we don't have an input for this frame yet
			end = count - 1
			continue
		}
		binary.BigEndian.PutUint32(buffer[currentByte:], g.GameData.Inputs[playerNumber][count])
		currentByte += 4
		buffer[currentByte] = g.GameData.Plugin[playerNumber][count]
		currentByte += 1
		count += 1
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
	g.GameData.PlayerAddresses[playerNumber] = addr
	if buf[0] == KEY_INFO_CLIENT {
		count := binary.BigEndian.Uint32(buf[2:])
		keys := binary.BigEndian.Uint32(buf[6:])
		plugin := buf[10]

		if len(g.GameData.PendingInputs[playerNumber]) == 0 {
			g.GameData.PendingInputs[playerNumber] = append(g.GameData.PendingInputs[playerNumber], keys)
			g.GameData.PendingPlugin[playerNumber] = append(g.GameData.PendingPlugin[playerNumber], plugin)
		}
		for i := 0; i < 4; i++ {
			if g.GameData.PlayerAddresses[i] != nil {
				g.sendUDPInput(count, g.GameData.PlayerAddresses[i], playerNumber, true)
			}
		}
	} else if buf[0] == PLAYER_INPUT_REQUEST {
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
				if len(g.GameData.SyncHash) > 500 {
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
		if err != nil {
			g.Logger.Error(err, "error reading UDP packet")
			defer g.UdpListener.Close()
			return
		}
		g.processUDP(addr, buf)
	}
}

func (g *GameServer) manageBuffer() {
	for i := 0; i < 4; i++ {
		if g.GameData.BufferHealth[i] != -1 {
			if g.GameData.BufferHealth[i] > BUFFER_TARGET && g.GameData.BufferSize[i] > 0 {
				g.GameData.BufferSize[i] -= 1
			} else if g.GameData.BufferHealth[i] < BUFFER_TARGET {
				g.GameData.BufferSize[i] += 1
			}
		}
	}
	time.Sleep(time.Second * 5)
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
	g.GameData.Inputs = make(map[byte]map[uint32]uint32)
	g.GameData.Plugin = make(map[byte]map[uint32]byte)
	g.GameData.PendingInputs = make(map[byte][]uint32)
	g.GameData.PendingPlugin = make(map[byte][]byte)
	g.GameData.SyncHash = make(map[uint32]uint64)

	go g.watchUDP()
	go g.manageBuffer()
	return g.Port
}
