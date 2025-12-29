# midi-monitor.tcl - A Tcl/Tk GUI for monitoring MIDI input and UDP output

package require Tk
package require udp

# --- Configuration ---
set addr 127.0.0.1
set port 60440
set wmidi_executable "wmidi.exe"

# --- UDP Socket Setup ---
set sock [udp_open]
fconfigure $sock -buffering none -translation binary
proc dest {addr port} {
  fconfigure $::sock -remote [list $addr $port]
}
dest $addr $port

# --- GUI Setup ---
wm withdraw . ;# Hide main window initially

toplevel .monitor
wm title .monitor "MIDI Monitor (wmidi.exe <-> skred)"
wm protocol .monitor WM_DELETE_WINDOW {
    catch {
        if {[info exists ::pipe]} {
            # Close the pipe, which should cause wmidi.exe to terminate
            catch {close $::pipe}
        }
    }
    catch {
        if {[info exists ::sock]} {
            catch {close $::sock}
        }
    }
    exit
}

grid rowconfigure .monitor 0 -weight 1
grid rowconfigure .monitor 2 -weight 1
grid columnconfigure .monitor 0 -weight 1

# Frame for wmidi.exe output
set wmidi_frame [ttk::labelframe .monitor.wmidi -text "wmidi.exe Output (Hex Bytes)"]
grid $wmidi_frame -row 0 -column 0 -sticky nsew -padx 5 -pady 5

grid rowconfigure $wmidi_frame 0 -weight 1
grid columnconfigure $wmidi_frame 0 -weight 1

set wmidi_text [text $wmidi_frame.text -height 10 -width 60 -state disabled]
grid $wmidi_text -row 0 -column 0 -sticky nsew
set wmidi_scroll [ttk::scrollbar $wmidi_frame.scroll -command "$wmidi_text yview"]
grid $wmidi_scroll -row 0 -column 1 -sticky ns
$wmidi_text configure -yscrollcommand "$wmidi_scroll set"

# Frame for UDP sent messages
set udp_frame [ttk::labelframe .monitor.udp -text "UDP Sent (Skode Messages)"]
grid $udp_frame -row 2 -column 0 -sticky nsew -padx 5 -pady 5

grid rowconfigure $udp_frame 0 -weight 1
grid columnconfigure $udp_frame 0 -weight 1

set udp_text [text $udp_frame.text -height 10 -width 60 -state disabled]
grid $udp_text -row 0 -column 0 -sticky nsew
set udp_scroll [ttk::scrollbar $udp_frame.scroll -command "$udp_text yview"]
grid $udp_scroll -row 0 -column 1 -sticky ns
$udp_text configure -yscrollcommand "$udp_scroll set"

# --- Logging Functions ---
proc log_wmidi {msg} {
    .monitor.wmidi.text configure -state normal
    .monitor.wmidi.text insert end "$msg\n"
    .monitor.wmidi.text see end
    .monitor.wmidi.text configure -state disabled
}

proc log_udp {msg} {
    .monitor.udp.text configure -state normal
    .monitor.udp.text insert end "$msg\n"
    .monitor.udp.text see end
    .monitor.udp.text configure -state disabled
}

# --- Modified wire procedure (logs and sends) ---
proc wire {msg} {
  log_udp $msg
  puts -nonewline $::sock $msg
  # For debugging, also print to console
  # puts "SENT -> $msg"
}

# --- wmidi.exe Pipe Handling ---
set pipe_running 0

proc start_wmidi_pipe {} {
    global wmidi_executable pipe pipe_running
    log_wmidi "Attempting to open pipe to $wmidi_executable..."
    set pipe [open "|$wmidi_executable" r]
    fconfigure $pipe -blocking 0 -buffering line

    set pipe_running 1
    fileevent $pipe readable [list process_wmidi_line $pipe]
    log_wmidi "Pipe to $wmidi_executable opened successfully. Waiting for MIDI data..."
}

proc process_wmidi_line {pipe} {
    if {[gets $pipe line] >= 0} {
        log_wmidi "RAW: $line"

        set bytes {}
        foreach hex $line { lappend bytes [expr "0x$hex"] }
        set status [lindex $bytes 0]
        set cmd     [expr {($status & 0xF0) >> 4}] ;# High nibble (Command)
        set channel [expr {($status & 0x0F) + 1}]  ;# Low nibble (Channel 1-16)

        switch $cmd {
            9 { ;# Note On
                set key [lindex $bytes 1]
                set vel [lindex $bytes 2]
                wire "v[expr $channel - 1] n$key l1"
            }
            8 { ;# Note Off
                set key [lindex $bytes 1]
                set vel [lindex $bytes 2]
                wire "v[expr $channel - 1] n$key l0"
            }
            14 { ;# Pitch Bend
                set d1 [lindex $bytes 1]
                set d2 [lindex $bytes 2]
                set val [expr {($d2 << 7) | $d1}]
                log_udp "Pitch Bend on Ch $channel: $val (not sent to skred)"
                # Original midi-bridge.tcl did not send Pitch Bend, keeping consistent
            }
            default {
                log_udp "Unhandled MIDI command $cmd on channel $channel"
            }
        }
    }
    if {[eof $pipe]} {
        log_wmidi "wmidi.exe pipe closed or encountered EOF. Exiting..."
        set pipe_running 0
        catch {close $pipe}
        # Optionally, try to restart or alert the user
        # after 1000 start_wmidi_pipe
    }
}

# --- Start everything ---
start_wmidi_pipe

wm deiconify .monitor ;# Show the monitor window
