package main

import (
	"bufio"
	"flag"
	"fmt"
	"net"
	"os"
	"strings"
	"time"
)

var (
	Width int
	RAddr string
	PAddr string
)

const (
	Reset      = "\033[0m"
	ColorWrong = "\033[31m"
	ColorRight = "\033[32m"
	ColorRedis = ColorRight
	ColorGrid  = "\033[36m"
	ColorTitle = ColorGrid
)

/* ================================================== */

type RedisClient net.TCPConn

func NewRedisClient(addr string) (*RedisClient, error) {
	tcp, err := net.ResolveTCPAddr("tcp", addr)
	if err != nil {
		return nil, err
	}
	conn, err := net.DialTCP("tcp", nil, tcp)
	if err != nil {
		return nil, err
	}
	return (*RedisClient)(conn), nil
}

func (rc *RedisClient) ExecuteCommand(cmd string) string {
	_, err := rc.Write([]byte(cmd + "\r\n"))
	if err != nil {
		return err.Error()
	}

	reader := bufio.NewReader(rc)
	res := strings.Builder{}
	for {
		err := rc.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
		if err != nil {
			res.WriteString(err.Error())
			break
		}

		bytes, err := reader.ReadBytes('\n')
		if err != nil {
			if netErr, ok := err.(net.Error); ok && netErr.Timeout() {

			} else {
				res.WriteString(err.Error())
			}
			break
		}
		res.WriteString(string(bytes))
	}
	return res.String()
}

/* ================================================== */

type Clients struct {
	redis  *RedisClient
	pikiwi *RedisClient
}

func NewClients(raddr, paddr string) (*Clients, error) {
	rc, err := NewRedisClient(raddr)
	if err != nil {
		return nil, err
	}
	pc, err := NewRedisClient(paddr)
	if err != nil {
		return nil, err
	}
	return &Clients{rc, pc}, nil
}

func (cs *Clients) Close() {
	cs.redis.Close()
	cs.pikiwi.Close()
}

func (cs *Clients) RunRepl() {
	scanner := bufio.NewScanner(os.Stdin)
	for {
		fmt.Print("cmd> ")
		scanner.Scan()
		cmd := scanner.Text()
		if strings.ToLower(cmd) == "exit" {
			break
		}
		rres := cs.redis.ExecuteCommand(cmd)
		pres := cs.pikiwi.ExecuteCommand(cmd)

		llines := strings.Split(pres, "\r\n")
		rlines := strings.Split(rres, "\r\n")

		maxLen := len("pikiwidb")
		for i := 0; i < len(llines); i++ {
			if len(llines[i]) > maxLen {
				maxLen = len(llines[i])
			}
		}

		cfmt := fmt.Sprintf("%%-%ds "+ColorGrid+"│ %%s"+Reset+"\n", maxLen+5)
		fmt.Printf(cfmt, ColorTitle+"pikiwidb", ColorTitle+"redis")
		for i := 0; i < len(llines)-1 && i < len(rlines)-1; i++ {
			lstr := ColorRight + llines[i]
			rstr := ColorRedis + rlines[i]
			if llines[i] != rlines[i] {
				lstr = ColorWrong + llines[i]
			}
			fmt.Printf(cfmt, lstr, rstr)
		}
	}
}

/* ================================================== */

func main() {
	flag.IntVar(&Width, "width", 20, "Width of each block")
	flag.StringVar(&RAddr, "redis", "127.0.0.1:6379", "ip and port of redis")
	flag.StringVar(&PAddr, "pikiwi", "127.0.0.1:9221", "ip and port of pikiwidb")
	flag.Parse()

	clients, err := NewClients(RAddr, PAddr)
	if err != nil {
		panic(err)
	}
	defer clients.Close()
	clients.RunRepl()
}
