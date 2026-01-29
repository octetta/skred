package main

import (
	"encoding/hex"
	"flag"
	"fmt"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"

	// Register image decoders
	_ "image/jpeg"
	_ "image/png"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/driver/desktop"
	"github.com/chzyer/readline"
)

var hexMode = false

func main() {
	isCLI := flag.Bool("cli", false, "internal flag for console mode")
	flag.Parse()

	if *isCLI {
		runUDPShell()
		return
	}

	a := app.NewWithID("com.udp.terminal.tool")
	a.SetIcon(resourceIconPng)

	if desk, ok := a.(desktop.App); ok {
		menu := fyne.NewMenu("UDP Tool",
			fyne.NewMenuItem("Open Shell", func() {
				launchSelfInTerminal()
			}),
			fyne.NewMenuItemSeparator(),
			fyne.NewMenuItem("Quit", func() {
				a.Quit()
			}),
		)
		desk.SetSystemTrayMenu(menu)
	}

	a.Run()
}

func launchSelfInTerminal() {
	self, _ := os.Executable()
	var cmd *exec.Cmd

	switch runtime.GOOS {
	case "windows":
		cmd = exec.Command("cmd.exe", "/c", "start", self, "--cli")
	case "darwin":
		cmd = exec.Command("open", "-a", "Terminal", self, "--args", "--cli")
	case "linux":
		terminals := []string{"ptyxis", "gnome-terminal", "gnome-console", "konsole", "xfce4-terminal", "xterm"}
		for _, term := range terminals {
			if _, err := exec.LookPath(term); err == nil {
				cmd = exec.Command(term, "--", self, "--cli")
				break
			}
		}
	}

	if cmd != nil {
		_ = cmd.Start()
	}
}

func runUDPShell() {
	home, _ := os.UserHomeDir()
	historyPath := filepath.Join(home, ".udp_shell_history")

	rl, err := readline.NewEx(&readline.Config{
		Prompt:      "\033[32mUDP-Shell>\033[0m ",
		HistoryFile: historyPath,
	})
	if err != nil {
		return
	}
	defer rl.Close()

	addr, _ := net.ResolveUDPAddr("udp", "127.0.0.1:60440")
	conn, err := net.DialUDP("udp", nil, addr)
	if err != nil {
		fmt.Printf("\033[31mConnection Error: %v\033[0m\n", err)
		return
	}
	defer conn.Close()

	// Asynchronous Receiver with Refresh Logic
	go func() {
		buf := make([]byte, 8192)
		for {
			n, _, err := conn.ReadFromUDP(buf)
			if err != nil {
				return
			}

			raw := buf[:n]
			var output string
			if hexMode {
				output = fmt.Sprintf("\033[36m[HEX]: %s\033[0m", hex.EncodeToString(raw))
			} else {
				output = strings.TrimRight(string(raw), "\r\n")
			}

			// THE FIX FOR BOTTOM-OF-WINDOW SCROLLING:
			// 1. Move to the start of the current line (\r)
			// 2. Clear the line (\033[K) so it doesn't leave "ghost" characters
			// 3. Print the server message
			// 4. Force a newline (\n)
			// 5. Use rl.Refresh() to redraw the prompt and user input on a fresh line
			fmt.Printf("\r\033[K%s\n", output)
			rl.Refresh()
			
			os.Stdout.Sync() 
		}
	}()

	fmt.Println("Shell Active (Port 60440). Type ^help for local commands.")

	for {
		line, err := rl.Readline()
		if err != nil {
			break
		}

		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		if strings.HasPrefix(line, "^") {
			cmdBody := strings.TrimPrefix(line, "^")
			response := myLocalFunction(cmdBody, rl)
			if response != "" {
				fmt.Printf("\033[33m%s\033[0m\n", response)
				os.Stdout.Sync()
			}
			continue
		}

		// Send entire line to UDP server
		_, _ = conn.Write([]byte(line + "\n"))
	}
}

func myLocalFunction(input string, rl *readline.Instance) string {
	input = strings.TrimSpace(input)
	lowerInput := strings.ToLower(input)

	if lowerInput == "hex" {
		hexMode = !hexMode
		status := "OFF"
		if hexMode { status = "ON" }
		return "Hex view is now " + status
	}

	if strings.HasPrefix(lowerInput, "prompt") {
		content := strings.TrimSpace(input[6:])
		content = strings.Trim(content, "\"'")
		if content == "" {
			return "Usage: ^prompt \"# \""
		}
		rl.SetPrompt(fmt.Sprintf("\033[35m%s\033[0m", content))
		return "Prompt updated."
	}

	switch lowerInput {
	case "help":
		return "Local Commands:\n  ^prompt \"text\" - Change prompt\n  ^hex           - Toggle hex-dump\n  ^cls           - Clear screen\n  ^exit          - Exit shell"
	case "version":
		return "UDP-Term v1.4.3 (Bottom-Scroll Fix)"
	case "cls":
		fmt.Print("\033[H\033[2J")
		return ""
	case "exit":
		os.Exit(0)
	}

	return "Unknown command: " + input
}
