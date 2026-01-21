package main

import (
	_ "embed"
	"fmt"
	"net"
	"strings"
	"time"

	. "modernc.org/tk9.0"
	. "modernc.org/tk9.0/extensions/eval"
)

//go:embed tkcon.tcl
var tkconScript string

// Global connection to maintain the same local port (like nc -u)
var udpConn *net.UDPConn

func main() {
	InitializeExtension("eval")

	// 1. Initialize UDP Connection once
	if err := initUDP("127.0.0.1:60440"); err != nil {
		fmt.Printf("Fatal UDP Error: %v\n", err)
		return
	}
	defer udpConn.Close()

	Eval("wm withdraw .")
	Eval(tkconScript)

	// Configure tkcon appearance
	Eval(`
		catch {
			::tkcon::OPT(exec) ""
			::tkcon::OPT(font) {Courier 10}
			::tkcon::OPT(showmultiple) 1
		}
		set ::pendingCommand ""
		set ::commandResult ""
	`)

	Eval("toplevel .gohidden")
	Eval("wm withdraw .gohidden")

	// Create the bridge button
	bridgeBtn := TButton(Command(func() {
		cmd, _ := Eval("set ::pendingCommand")
		if strings.TrimSpace(cmd) != "" {
			result := processCommand(cmd)
			
			// Safely escape result for Tcl
			escaped := strings.ReplaceAll(result, "\\", "\\\\")
			escaped = strings.ReplaceAll(escaped, "{", "\\{")
			escaped = strings.ReplaceAll(escaped, "}", "\\}")
			
			Eval(fmt.Sprintf("set ::commandResult {%s}", escaped))
			Eval("set ::pendingCommand \"\"")
		}
	}))

	// Fix: Access Window field directly (no parens)
	btnPath := bridgeBtn.Window.String()
	Eval(fmt.Sprintf("pack %s -in .gohidden", btnPath))
	
	// Get the Tcl command bound to the button
	cmdString, _ := Eval(fmt.Sprintf("%s cget -command", btnPath))

	// Override tkcon to route through Go
	Eval(fmt.Sprintf(`
		if {[info commands ::tkcon::EvalCmd_orig] eq ""} {
			rename ::tkcon::EvalCmd ::tkcon::EvalCmd_orig
		}
		proc ::tkcon::EvalCmd {w cmd} {
			$w mark set output end
			if {$cmd ne ""} { history add $cmd }
			set ::pendingCommand $cmd
			set ::commandResult ""
			
			# Trigger Go callback
			%s

			set result $::commandResult
			if {$result ne ""} {
				$w insert output "$result\n" stdout
			}
			::tkcon::Prompt
		}
	`, cmdString))

	Eval("tkcon show")
	Eval(`puts "Connected to 127.0.0.1:60440 (Persistent Port)"`)
	
	App.Wait()
}

func initUDP(target string) error {
	remoteAddr, err := net.ResolveUDPAddr("udp", target)
	if err != nil {
		return err
	}
	// DialUDP without a local addr lets the OS pick a port, 
	// but keeping the 'conn' open keeps that port reserved.
	conn, err := net.DialUDP("udp", nil, remoteAddr)
	if err != nil {
		return err
	}
	udpConn = conn
	return nil
}

func processCommand(code string) string {
	// 1. Drain any stale data that arrived between commands
	udpConn.SetReadDeadline(time.Now().Add(1 * time.Millisecond))
	drainBuf := make([]byte, 2048)
	for {
		_, err := udpConn.Read(drainBuf)
		if err != nil {
			break
		}
	}

	// 2. Send command with newline
	_, err := fmt.Fprintln(udpConn, code)
	if err != nil {
		return "Send Error: " + err.Error()
	}

	// 3. Read Response
	var fullResult strings.Builder
	readBuf := make([]byte, 4096)
	
	// First read: wait up to 2 seconds for the server to start talking
	udpConn.SetReadDeadline(time.Now().Add(250 * time.Millisecond))
	n, err := udpConn.Read(readBuf)
	if err != nil {
		if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
			return ""
		}
		return "Read Error: " + err.Error()
	}
	fullResult.Write(readBuf[:n])

	// Subsequent reads: collect any extra packets (dialog style)
	for {
		udpConn.SetReadDeadline(time.Now().Add(100 * time.Millisecond))
		n, err := udpConn.Read(readBuf)
		if err != nil {
			break // Likely a timeout, meaning no more data for now
		}
		fullResult.Write(readBuf[:n])
	}

	return strings.TrimSpace(fullResult.String())
}
