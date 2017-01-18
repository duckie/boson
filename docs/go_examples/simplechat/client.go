package main

import "net"
import "fmt"

func clientRead(conn net.Conn) {
  fmt.Println("Start read")
  buf := make([]byte, 2048)
  for {
    conn.Read(buf)
  }
}

func clientWrite(conn net.Conn) {
  fmt.Println("Start write")
  for {
    conn.Write([]byte("message"))
  }
}

func main() {
  i := 0
  for i < 10 {
    conn, err := net.Dial("tcp", "127.0.0.1:8080")
    if (err == nil) {
      go clientWrite(conn)
      go clientRead(conn)
      i++
    }
  }
  fmt.Println("Finished launching clients")
  blocker := make(chan int)
  <- blocker
}
