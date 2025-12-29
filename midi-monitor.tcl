# midi-monitor.tcl - A Tcl/Tk GUI for monitoring MIDI input and UDP output

package require Tk
package require udp

# --- Configuration ---
set addr 127.0.0.1
set port 60440
#set skmidi_executable "skmidi.exe"
set skmidi_executable "./skmidi"

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
wm title .monitor "MIDI Monitor (skmidi <-> skred)"
wm protocol .monitor WM_DELETE_WINDOW {
    catch {
        if {[info exists ::pipe]} {
            # Close the pipe, which should cause skmidi to terminate
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

# Frame for skmidi output
set skmidi_frame [ttk::labelframe .monitor.skmidi -text "skmidi Output (Hex Bytes)"]
grid $skmidi_frame -row 0 -column 0 -sticky nsew -padx 5 -pady 5

grid rowconfigure $skmidi_frame 0 -weight 1
grid columnconfigure $skmidi_frame 0 -weight 1

set skmidi_text [text $skmidi_frame.text -height 10 -width 60 -state disabled]
grid $skmidi_text -row 0 -column 0 -sticky nsew
set skmidi_scroll [ttk::scrollbar $skmidi_frame.scroll -command "$skmidi_text yview"]
grid $skmidi_scroll -row 0 -column 1 -sticky ns
$skmidi_text configure -yscrollcommand "$skmidi_scroll set"

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
proc log_skmidi {msg} {
    .monitor.skmidi.text configure -state normal
    .monitor.skmidi.text insert end "$msg\n"
    .monitor.skmidi.text see end
    .monitor.skmidi.text configure -state disabled
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

# --- skmidi Pipe Handling ---
set pipe_running 0

proc start_skmidi_pipe {} {
    global skmidi_executable pipe pipe_running
    log_skmidi "Attempting to open pipe to $skmidi_executable..."
    set pipe [open "|$skmidi_executable" r]
    fconfigure $pipe -blocking 0 -buffering line

    set pipe_running 1
    fileevent $pipe readable [list process_skmidi_line $pipe]
    log_skmidi "Pipe to $skmidi_executable opened successfully. Waiting for MIDI data..."
}

proc process_skmidi_line {pipe} {
    if {[gets $pipe line] >= 0} {
        log_skmidi "RAW: $line"

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
        log_skmidi "skmidi pipe closed or encountered EOF. Exiting..."
        set pipe_running 0
        catch {close $pipe}
        # Optionally, try to restart or alert the user
        # after 1000 start_skmidi_pipe
    }
}

# --- Start everything ---
start_skmidi_pipe

wm deiconify .monitor ;# Show the monitor window
