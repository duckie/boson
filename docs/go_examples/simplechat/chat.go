package main

import "net"
import "time"
import "fmt"
import "sync/atomic"
import "math/rand"

func listenClient(clientConn net.Conn, broadcastChan chan string) {
  buf := make([]byte, 2048)
  defer clientConn.Close()
  for {
    nread, err := clientConn.Read(buf)
    if (err != nil || nread < 1) {
      return 
    }
    message := string(buf[:nread-1]) // Looks like it reads \r\n
    if (message == "quit") {
      return 
    }

    broadcastChan <- message;
  }
}

func handleNewConnections(newConnChan chan net.Conn) {
  socket, _ := net.Listen("tcp", ":8080") 
  for {
    rwc, err := socket.Accept()
    if (err == nil) {
      newConnChan <- rwc 
    }
  }
}

func displayCount(counter * int64) {
  latest := time.Now()
  stepDuration := time.Second
  for {
    fmt.Println(atomic.SwapInt64(counter,0)*time.Second.Nanoseconds()/stepDuration.Nanoseconds())
    time.Sleep(time.Second)
    current := time.Now()
    stepDuration = current.Sub(latest)
    latest = current
  }
}

func main() {
  newConnChan := make(chan net.Conn,1)
  broadcastChan := make(chan string,1)
  go handleNewConnections(newConnChan)
  connections := make([]net.Conn,0)
  var counter int64 = 0

  go displayCount(&counter)

  for {
    select {
      case newConn := <- newConnChan:
        connections = append(connections, newConn)
        go listenClient(newConn, broadcastChan)
      case message := <- broadcastChan:
        connections[rand.Intn(len(connections))].Write([]byte(message + "\n"))
        atomic.AddInt64(&counter,1)
    }
  }
}
