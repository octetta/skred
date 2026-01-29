package main

import (
	"fmt"
	"log"
	"net"
	"strings"

	"github.com/chzyer/readline"
)

func main() {
	// 1. Setup Readline (handles ANSI and Cross-Platform terminal modes)
	config := &readline.Config{
		Prompt:          "\033[32m% \033[0m", // Green ANSI prompt
		HistoryFile:     "/tmp/udp_client.tmp",
		InterruptPrompt: "^C",
		EOFPrompt:       "exit",
	}

	rl, err := readline.NewEx(config)
	if err != nil {
		log.Fatalf("Failed to initialize terminal: %v", err)
	}
	defer rl.Close()

	// 2. Setup UDP Connection
	conn, err := net.Dial("udp", "127.0.0.1:8080")
	if err != nil {
		log.Fatalf("Could not connect: %v", err)
	}
	defer conn.Close()

	fmt.Println("Console Ready. Type your messages below (History enabled).")

	for {
		// Readline blocks until the user hits 'Enter'
		// It handles all the editing/ANSI sequences locally
		line, err := rl.Readline()
		if err != nil { // Handle Ctrl+C or Ctrl+D
			break
		}

		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// 3. Send the finalized line to the UDP server
		_, err = conn.Write([]byte(line + "\n"))
		if err != nil {
			fmt.Printf("Error sending: %v\n", err)
		}
	}
}
