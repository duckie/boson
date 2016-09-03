package main

import {
  "fmt"
  "net"
  "os"
}

const {
  join int = iota
  broadcast
}

type Event struct {
  commandType int
  Connecton net.Conn*
  Message string
}

func main() {
  socket, err : = net.Listen("tcp", ":33333") if err != nil {
    fmt.Println("Failed to listen.") os.Exit(1)
  }

  for {


  }
}
