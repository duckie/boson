package main

import "net"
import "time"
import "fmt"
import "sync/atomic"

func listenClient(clientConn net.Conn, broadcastChan chan string) {
  buf := make([]byte, 2048)
  for {
    nread, err := clientConn.Read(buf)
    if (err != nil || 0 == nread) {
      clientConn.Close()
      return 
    }
    message := string(buf[:nread-2]) // Looks like it reads \r\n
    if (message == "quit") {
      clientConn.Close()
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

func displayCount(counter * uint64) {
  for {
    fmt.Println(atomic.SwapUint64(counter,0))
    time.Sleep(1*time.Second)
  }
}

func main() {
  newConnChan := make(chan net.Conn,1)
  broadcastChan := make(chan string,1)
  go handleNewConnections(newConnChan)
  connections := make(map[net.Conn]bool)
  var counter uint64 = 0

  go displayCount(&counter)

  for {
    select {
      case newConn := <- newConnChan:
        connections[newConn] = true
        go listenClient(newConn, broadcastChan)
      case message := <- broadcastChan:
        for conn, _ := range connections {
          conn.Write([]byte(message + "\n"))
          atomic.AddUint64(&counter,1)
        }
    }
  }
}
