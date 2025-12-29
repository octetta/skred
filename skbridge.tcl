# skbridge.tcl - skred-bridge: MIDI monitor → UDP bridge
package require Tk
package require udp

# ------------------------------------------------------------
# Environment detection and skmidi handling – FINAL FIXED VERSION
# ------------------------------------------------------------
if {[info commands ::starkit::startup] ne ""} {
    package require starkit
    starkit::startup

    if {[info exists starkit::topdir]} {
        set is_bundled 1
        set appdir $starkit::topdir
    } else {
        set is_bundled 0
        set appdir [file dirname [info script]]
    }
} else {
    set is_bundled 0
    set appdir [file dirname [info script]]
}

cd $appdir
wm title . "skred-bridge"

# Platform-specific name
if {$tcl_platform(platform) eq "windows"} {
    set skmidi_name "skmidi.exe"
} else {
    set skmidi_name "skmidi"
}

# Handle bundled vs source
if {$is_bundled} {
    # Bundled (starpack/starkit) – extract to temp on ALL platforms for safety
    set bundled_skmidi [file join $appdir $skmidi_name]
    set temp_dir [file join [file normalize ~/AppData/Local/Temp] skred-bridge-[pid]] ;# Windows-friendly temp
    if {$tcl_platform(platform) ne "windows"} {
        # Unix temp
        set temp_dir "/tmp/skred-bridge-[pid]"
    }
    file mkdir $temp_dir
    set skmidi_path [file join $temp_dir $skmidi_name]
    file copy -force $bundled_skmidi $skmidi_path
    file attributes $skmidi_path -permissions ugo+x   ;# Ensure executable on Unix

    proc cleanup {} {
        global midi_pipe sock temp_dir
        catch {close $midi_pipe}
        catch {close $sock}
        catch {file delete -force $temp_dir}
    }
} else {
    # Source run
    set skmidi_path [file join $appdir $skmidi_name]
    file attributes $skmidi_path -permissions ugo+x

    proc cleanup {} {
        global midi_pipe sock
        catch {close $midi_pipe}
        catch {close $sock}
    }
}

if {![file exists $skmidi_path]} {
    tk_messageBox -icon error -title "Missing skmidi" \
        -message "Cannot find '$skmidi_name' at expected location."
    exit 1
}

# ------------------------------------------------------------
# UDP Setup
# ------------------------------------------------------------
set addr 127.0.0.1
set port 60440
set sock [udp_open]
fconfigure $sock -buffering none -translation binary
fconfigure $sock -remote [list $addr $port]

# ------------------------------------------------------------
# GUI Setup
# ------------------------------------------------------------
wm withdraw .
toplevel .monitor
wm title .monitor "MIDI → skode (skred-bridge)"
wm protocol .monitor WM_DELETE_WINDOW {
    cleanup
    destroy .monitor
    exit 0   ;# Forces full exit even under tclsh
}

grid rowconfigure .monitor 0 -weight 1
grid rowconfigure .monitor {1 2 3} -weight 0
grid columnconfigure .monitor 0 -weight 1

# skmidi input frame
ttk::labelframe .monitor.skmidi -text "from skmidi"
grid .monitor.skmidi -row 0 -column 0 -sticky nsew -padx 5 -pady 5
text .monitor.skmidi.text -state disabled -height 12
ttk::scrollbar .monitor.skmidi.sb -command ".monitor.skmidi.text yview"
grid .monitor.skmidi.text -row 0 -column 0 -sticky nsew
grid .monitor.skmidi.sb -row 0 -column 1 -sticky ns
.monitor.skmidi.text configure -yscrollcommand ".monitor.skmidi.sb set"
grid rowconfigure .monitor.skmidi 0 -weight 1
grid columnconfigure .monitor.skmidi 0 -weight 1

# Transform frames
ttk::labelframe .monitor.noteon -text "NOTE ON transform"
grid .monitor.noteon -row 1 -column 0 -sticky ew -padx 5 -pady 3
ttk::label .monitor.noteon.l -text "(e.g. v\$c n\$n l\$v):"
ttk::entry .monitor.noteon.e -textvariable ::note_on_transform_string -width 50
grid .monitor.noteon.l -row 0 -column 0 -sticky w -padx 5
grid .monitor.noteon.e -row 0 -column 1 -sticky ew -padx 5
grid columnconfigure .monitor.noteon 1 -weight 1
set ::note_on_transform_string "v\$c n\$n l\$v"

ttk::labelframe .monitor.noteoff -text "NOTE OFF transform"
grid .monitor.noteoff -row 2 -column 0 -sticky ew -padx 5 -pady 3
ttk::label .monitor.noteoff.l -text "(e.g. v\$c n\$n l\$v):"
ttk::entry .monitor.noteoff.e -textvariable ::note_off_transform_string -width 50
grid .monitor.noteoff.l -row 0 -column 0 -sticky w -padx 5
grid .monitor.noteoff.e -row 0 -column 1 -sticky ew -padx 5
grid columnconfigure .monitor.noteoff 1 -weight 1
set ::note_off_transform_string "v\$c n\$n l\$v"

ttk::labelframe .monitor.bend -text "PITCH BEND transform"
grid .monitor.bend -row 3 -column 0 -sticky ew -padx 5 -pady 3
ttk::label .monitor.bend.l -text "(e.g. v\$c b\$b B\$B):"
ttk::entry .monitor.bend.e -textvariable ::pitch_bend_transform_string -width 50
grid .monitor.bend.l -row 0 -column 0 -sticky w -padx 5
grid .monitor.bend.e -row 0 -column 1 -sticky ew -padx 5
grid columnconfigure .monitor.bend 1 -weight 1
set ::pitch_bend_transform_string "v\$c N\$b"

# UDP output frame
ttk::labelframe .monitor.udp -text "UDP → skred"
grid .monitor.udp -row 4 -column 0 -sticky nsew -padx 5 -pady 5
text .monitor.udp.text -state disabled -height 12
ttk::scrollbar .monitor.udp.sb -command ".monitor.udp.text yview"
grid .monitor.udp.text -row 0 -column 0 -sticky nsew
grid .monitor.udp.sb -row 0 -column 1 -sticky ns
.monitor.udp.text configure -yscrollcommand ".monitor.udp.sb set"
grid rowconfigure .monitor.udp 0 -weight 1
grid columnconfigure .monitor.udp 0 -weight 1

# ------------------------------------------------------------
# Logging
# ------------------------------------------------------------
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

proc wire {msg} {
    log_udp $msg
    puts -nonewline $::sock $msg
}

# ------------------------------------------------------------
# skmidi Pipe
# ------------------------------------------------------------
proc start_skmidi_pipe {} {
    global skmidi_path midi_pipe
    set cmd [string map {\\ /} [file nativename $skmidi_path]]  ;# Forward slashes fix Windows pipe issues

    log_skmidi "Launching: $cmd"
    set midi_pipe [open "|$cmd" r]
    fconfigure $midi_pipe -blocking 0 -buffering line
    fileevent $midi_pipe readable [list process_skmidi_line]
    log_skmidi "Waiting for MIDI input..."
}

proc process_skmidi_line {} {
    global midi_pipe
    if {[eof $midi_pipe]} {
        log_skmidi "skmidi terminated."
        catch {close $midi_pipe}
        return
    }
    if {[gets $midi_pipe line] < 0} return

    log_skmidi $line
    if {[llength [set bytes [split $line]]] != 3} return

    foreach hex $bytes { lappend vals [expr "0x$hex"] }
    lassign $vals status d1 d2

    set cmd   [expr {($status >> 4) & 0xF}]
    set chan  [expr {($status & 0xF) + 1}]   ;# 1-based
    set c     [expr {$chan - 1}]             ;# 0-based for skode

    switch $cmd {
        9 { ;# Note On
            set n $d1
            set v [format %.3f [expr {$d2 / 127.0}]]
            set msg $::note_on_transform_string
        }
        8 { ;# Note Off
            set n $d1
            set v [format %.3f [expr {$d2 / 127.0}]]
            set msg $::note_off_transform_string
        }
        14 { ;# Pitch Bend
            set raw [expr {($d2 << 7) | $d1}]
            set b   [format %.3f [expr {($raw - 8192) / 8192.0}]]
            set B   $raw
            set msg $::pitch_bend_transform_string
            regsub -all {\$b} $msg $b msg
            regsub -all {\$B} $msg $B msg
        }
        default { return }
    }

    # Common substitutions
    regsub -all {\$c} $msg $c msg
    regsub -all {\$n} $msg $n msg
    regsub -all {\$v} $msg $v msg

    wire $msg
}

# ------------------------------------------------------------
# Cleanup override
# ------------------------------------------------------------
rename exit ::real_exit
proc exit {{code 0}} {
    cleanup
    destroy .monitor
    ::real_exit $code
}

# ------------------------------------------------------------
# Start
# ------------------------------------------------------------
start_skmidi_pipe
wm deiconify .monitor
vwait forever   ;# Keep the app running