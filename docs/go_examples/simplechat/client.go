package main

import "net"
import "fmt"
import "sync/atomic"
import "time"
import "strconv"
import "os"

func clientRead(conn net.Conn, counter *int64) {
	buf := make([]byte, 2048)
	for {
		nread, err := conn.Read(buf)
		if err == nil && 0 < nread {
			atomic.AddInt64(counter, 1)
		} else {
      return
    }
	}
}

func clientWrite(conn net.Conn, counter *int64) {
	for {
		nwritten, err := conn.Write([]byte("message"))
		if err == nil && 0 < nwritten {
			atomic.AddInt64(counter, 1)
		} else {
      return
    }
	}
}

func main() {
	nb_iter, _ := strconv.ParseInt(os.Args[1], 10, 64)
	var nread int64
	var nwrite int64
	var i int64

	go func(nread *int64, nwrite *int64, nconnected *int64, size int64) {
		latest := time.Now()
		stepDuration := time.Second
		for {
			fmt.Printf("r/s:%9d w/s:%9d clients: %d/%d\n", atomic.SwapInt64(nread, 0)*time.Second.Nanoseconds()/stepDuration.Nanoseconds(), atomic.SwapInt64(nwrite, 0)*time.Second.Nanoseconds()/stepDuration.Nanoseconds(), atomic.LoadInt64(nconnected), size)
			time.Sleep(time.Second)
			current := time.Now()
			stepDuration = current.Sub(latest)
			latest = current
		}
	}(&nread, &nwrite, &i, nb_iter)

	for i < nb_iter {
		conn, err := net.Dial("tcp", "127.0.0.1:8080")
		if err == nil {
			go clientRead(conn, &nread)
			go clientWrite(conn, &nwrite)
			i++
		}
	}

	fmt.Println("Finished launching clients")
	blocker := make(chan int)
	<-blocker
}
