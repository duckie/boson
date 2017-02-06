package main

import "net"
import "fmt"
import "sync/atomic"
import "time"

func clientRead(conn net.Conn, counter *uint64) {
  fmt.Println("Start read")
  for {
    buf := make([]byte, 2048)
    nread, err := conn.Read(buf)
    if (err == nil && 0 < nread) {
      atomic.AddUint64(counter,1)
    }
  }
}

func clientWrite(conn net.Conn, counter *uint64) {
  fmt.Println("Start write")
  for {
    nwritten, err := conn.Write([]byte("message"))
    if (err == nil && 0 < nwritten) {
      atomic.AddUint64(counter,1)
    }
  }
}

func main() {
  nb_iter := 10
  i := 0
  wcounters := make([]uint64,nb_iter)
  rcounters := make([]uint64,nb_iter)
  for i < nb_iter {
    rcounters[i] = 0
    wcounters[i] = 0
    i++
  }
  i=0
  for i < nb_iter {
    conn, err := net.Dial("tcp", "127.0.0.1:8080")
    if (err == nil) {
      go clientWrite(conn,&wcounters[i])
      go clientRead(conn,&rcounters[i])
      i++
    }
  }

  go func(rcounters []uint64, wcounters []uint64, size int) {
    for {
      for i := 0; i < nb_iter; i++ {
        fmt.Printf("(%9d/%9d) ", atomic.LoadUint64(&rcounters[i]), atomic.LoadUint64(&wcounters[i]))
      }
      fmt.Printf("\n") 
      time.Sleep(1*time.Second)
    }
  }(rcounters,wcounters,nb_iter)

  fmt.Println("Finished launching clients")
  blocker := make(chan int)
  <- blocker
}
