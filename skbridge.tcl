# skbridge.tcl - skred-bridge: MIDI monitor → UDP bridge (stable, fixed)
package require Tk
package require udp

# ------------------------------------------------------------
# Robust bundled detection and skmidi setup
# ------------------------------------------------------------
set is_bundled 0
set appdir ""

if {[info commands ::starkit::startup] ne ""} {
    package require starkit
    starkit::startup
    if {[info exists ::starkit::topdir]} {
        set is_bundled 1
        set appdir $::starkit::topdir
    }
}

if {!$is_bundled && [string match "*skred-bridge*" [file tail [info nameofexecutable]]]} {
    set is_bundled 1
    set appdir [info nameofexecutable]
}

if {!$is_bundled} {
    set appdir [file dirname [info script]]
    cd $appdir
}

wm title . "skred-bridge"

if {$tcl_platform(platform) eq "windows"} {
    set skmidi_name "skmidi.exe"
} else {
    set skmidi_name "skmidi"
}

set ::skmidi_pid ""

if {$is_bundled} {
    set bundled_skmidi [file join $appdir $skmidi_name]

    if {[info exists env(TEMP)] && [file isdirectory $env(TEMP)] && [file writable $env(TEMP)]} {
        set temp_base $env(TEMP)
    } else {
        set temp_base [file normalize ~]
    }
    set temp_dir [file join $temp_base "skred-bridge-[pid]"]
    catch {file mkdir $temp_dir}

    set skmidi_path [file join $temp_dir $skmidi_name]

    if {[catch {file copy -force $bundled_skmidi $skmidi_path} copy_err]} {
        tk_messageBox -icon error -title "Extraction Failed" -message "Could not extract $skmidi_name:\n$copy_err"
        exit 1
    }
    if {![file exists $skmidi_path] || [file size $skmidi_path] == 0} {
        tk_messageBox -icon error -title "Extraction Failed" -message "Extracted file invalid: $skmidi_path"
        exit 1
    }

    proc cleanup {} {
        global midi_pipe sock temp_dir skmidi_pid
        kill_skmidi
        catch {close $midi_pipe}
        catch {close $sock}
        catch {file delete -force $temp_dir}
    }
} else {
    set skmidi_path [file join $appdir $skmidi_name]
    if {$tcl_platform(platform) ne "windows"} {
        catch {file attributes $skmidi_path -permissions ugo+x}
    }

    proc cleanup {} {
        global midi_pipe sock skmidi_pid
        kill_skmidi
        catch {close $midi_pipe}
        catch {close $sock}
    }
}

if {![file exists $skmidi_path]} {
    tk_messageBox -icon error -title "skmidi Missing" -message "Cannot find $skmidi_name at:\n$skmidi_path"
    exit 1
}

proc kill_skmidi {} {
    global skmidi_pid
    if {$skmidi_pid ne "" && $::tcl_platform(platform) ne "windows"} {
        catch {exec kill $skmidi_pid}
        set skmidi_pid ""
    }
}

# ------------------------------------------------------------
# UDP Setup
# ------------------------------------------------------------
set ::addr 127.0.0.1
set ::port 60440
set ::sock [udp_open]

set ::config_addr $::addr
set ::config_port $::port
set ::custom_transform ""

fconfigure $::sock -buffering none -translation binary
fconfigure $::sock -remote [list $::addr $::port]

set ::note_on_transform_string "v\$c n\$n l\$v"
set ::note_off_transform_string "v\$c n\$n l\$v"
set ::pitch_bend_transform_string "v\$c N\$b"

# ------------------------------------------------------------
# GUI
# ------------------------------------------------------------
wm withdraw .
toplevel .monitor
wm title .monitor "MIDI to skode"
wm protocol .monitor WM_DELETE_WINDOW {
    cleanup
    destroy .monitor
    exit 0
}

grid rowconfigure .monitor {0 4} -weight 1
grid rowconfigure .monitor {1 2 3} -weight 0
grid columnconfigure .monitor 0 -weight 1

ttk::labelframe .monitor.skmidi -text "from MIDI"
grid .monitor.skmidi -row 0 -column 0 -sticky nsew -padx 5 -pady 5
text .monitor.skmidi.text -state disabled
ttk::scrollbar .monitor.skmidi.sb -command ".monitor.skmidi.text yview"
grid .monitor.skmidi.text -row 0 -column 0 -sticky nsew
grid .monitor.skmidi.sb -row 0 -column 1 -sticky ns
.monitor.skmidi.text configure -yscrollcommand ".monitor.skmidi.sb set"
grid rowconfigure .monitor.skmidi 0 -weight 1
grid columnconfigure .monitor.skmidi 0 -weight 1

foreach {r title var label} {
    1 "NOTE ON" ::note_on_transform_string "(e.g. v\$c n\$n l\$v):"
    2 "NOTE OFF" ::note_off_transform_string "(e.g. v\$c n\$n l\$v):"
    3 "PITCH BEND" ::pitch_bend_transform_string "(e.g. v\$c N\$b):"
} {
    ttk::labelframe .monitor.tf$r -text $title
    grid .monitor.tf$r -row $r -column 0 -sticky ew -padx 5 -pady 3
    ttk::label .monitor.tf$r.l -text $label
    ttk::entry .monitor.tf$r.e -textvariable $var -width 60
    grid .monitor.tf$r.l -row 0 -column 0 -sticky w -padx 5
    grid .monitor.tf$r.e -row 0 -column 1 -sticky ew -padx 5
    grid columnconfigure .monitor.tf$r 1 -weight 1
}

ttk::labelframe .monitor.udp -text "to skred"
grid .monitor.udp -row 4 -column 0 -sticky nsew -padx 5 -pady 5
text .monitor.udp.text -state disabled
ttk::scrollbar .monitor.udp.sb -command ".monitor.udp.text yview"
grid .monitor.udp.text -row 0 -column 0 -sticky nsew
grid .monitor.udp.sb -row 0 -column 1 -sticky ns
.monitor.udp.text configure -yscrollcommand ".monitor.udp.sb set"
grid rowconfigure .monitor.udp 0 -weight 1
grid columnconfigure .monitor.udp 0 -weight 1

###
ttk::labelframe .monitor.comm -text "config"
grid .monitor.comm -row 5 -column 0 -sticky ew -padx 5 -pady 5

ttk::label .monitor.comm.addr_label -text "address"
ttk::entry .monitor.comm.addr_entry -textvariable ::config_addr -width 20
ttk::label .monitor.comm.port_label -text "port"
ttk::entry .monitor.comm.port_entry -textvariable ::config_port -width 10
ttk::button .monitor.comm.update_btn -text "update" -command update_udp_target

grid .monitor.comm.addr_label -row 0 -column 0 -sticky w -padx 5 -pady 5
grid .monitor.comm.addr_entry -row 0 -column 1 -sticky ew -padx 5 -pady 5
grid .monitor.comm.port_label -row 0 -column 2 -sticky w -padx 5 -pady 5
grid .monitor.comm.port_entry -row 0 -column 3 -sticky ew -padx 5 -pady 5
grid .monitor.comm.update_btn -row 0 -column 4 -sticky e -padx 5 -pady 5

grid columnconfigure .monitor.comm 1 -weight 1
grid columnconfigure .monitor.comm 3 -weight 1
###
ttk::label .monitor.comm.transform_label -text "custom"
ttk::entry .monitor.comm.transform_entry -textvariable ::custom_transform -width 40

grid .monitor.comm.transform_label -row 1 -column 0 -sticky w -padx 5 -pady 5
grid .monitor.comm.transform_entry -row 1 -column 1 -columnspan 3 -sticky ew -padx 5 -pady 5
###

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

proc update_udp_target {} {
    global sock config_addr config_port addr port
    
    # Validate inputs
    if {[string trim $config_addr] eq ""} {
        tk_messageBox -icon error -title "Invalid Address" -message "Address cannot be empty"
        return
    }
    if {![string is integer -strict $config_port] || $config_port < 1 || $config_port > 65535} {
        tk_messageBox -icon error -title "Invalid Port" -message "Port must be between 1 and 65535"
        return
    }
    
    # Update the socket configuration
    set addr $config_addr
    set port $config_port
    
    catch {close $sock}
    set sock [udp_open]
    fconfigure $sock -buffering none -translation binary
    fconfigure $sock -remote [list $addr $port]
    
    log_udp "Updated target to ${addr}:${port}"
}

proc wire {msg} {
    log_udp $msg
    puts -nonewline $::sock $msg
}

# ------------------------------------------------------------
# Safe math support using ${...} in transform strings
# ------------------------------------------------------------
proc safe_eval {expr_str} {
    if {[string trim $expr_str] eq ""} {
        return "ERR"
    }
    if {[catch {expr $expr_str} result]} {
        return "ERR"
    }
    return $result
}

proc expand_string {input_string} {
    # Only upvar variables that exist in caller's scope
    foreach var {a c n v} {
        if {[uplevel 1 [list info exists $var]]} {
            upvar 1 $var $var
        }
    }
    
    set result ""
    set i 0
    set len [string length $input_string]
    
    while {$i < $len} {
        set char [string index $input_string $i]
        
        if {$char eq "\$"} {
            incr i
            set next_char [string index $input_string $i]
            
            if {$next_char eq "\{"} {
                incr i
                set expr_start $i
                set brace_count 1
                
                while {$brace_count > 0} {
                    set char [string index $input_string $i]
                    if {$char eq "\{"} {incr brace_count}
                    if {$char eq "\}"} {incr brace_count -1}
                    incr i
                }
                
                set expr [string range $input_string $expr_start [expr {$i - 2}]]
                set map_list {}
                foreach var {a c n v} {
                    if {[info exists $var]} {
                        lappend map_list "\$$var" [set $var]
                    }
                }
                set expr [string map $map_list $expr]
                append result [expr $expr]
            } else {
                set var_name ""
                while {$i < $len && [string is alnum [string index $input_string $i]]} {
                    append var_name [string index $input_string $i]
                    incr i
                }
                if {[info exists $var_name]} {
                    append result [set $var_name]
                }
                continue
            }
        } else {
            append result $char
            incr i
        }
    }
    
    return $result
}

# ------------------------------------------------------------
# skmidi pipe
# ------------------------------------------------------------

proc start_skmidi_pipe {} {
    global skmidi_path midi_pipe skmidi_pid
    set native [file nativename $skmidi_path]
    set cmd [string map {\\ /} $native]

    log_skmidi "Launching skmidi: $cmd"
    set midi_pipe [open "|$cmd" r+]
    set skmidi_pid [pid $midi_pipe]
    fconfigure $midi_pipe -blocking 0 -buffering line
    fileevent $midi_pipe readable [list process_skmidi_line]
}

###
proc apply_custom_transform {c_val n_val v_val b_val B_val} {
    global custom_transform
    
    # If no custom transform, return original values
    if {[string trim $custom_transform] eq ""} {
        return [list $c_val $n_val $v_val $b_val $B_val]
    }
    
    # Create local variables for the transform
    set c $c_val
    set n $n_val
    set v $v_val
    set b $b_val
    set B $B_val
    
    # Try to execute the custom transform
    if {[catch {eval $custom_transform} err]} {
        log_udp "Custom transform error: $err"
        return [list $c_val $n_val $v_val $b_val $B_val]
    }
    
    # Return the potentially modified values
    return [list $c $n $v $b $B]
}
###

proc process_skmidi_line {} {
    global midi_pipe
    if {[eof $midi_pipe]} {
        log_skmidi "skmidi terminated"
        catch {close $midi_pipe}
        set ::skmidi_pid ""
        return
    }
    if {[gets $midi_pipe line] < 0} return

    log_skmidi $line
    set bytes [split $line]
    if {[llength $bytes] != 3} return

    lassign [lmap h $bytes {expr "0x$h"}] status data1 data2
    set cmd [expr {($status >> 4) & 0xF}]
    set c [expr {$status & 0xF}]

    set msg ""
    set v 0
    set b 0
    set raw 0

    switch $cmd {
        9 {
            set v [format %.3f [expr {$data2 / 127.0}]]
            set msg $::note_on_transform_string
        }
        8 {
            set v [format %.3f [expr {$data2 / 127.0}]]
            set msg $::note_off_transform_string
        }
        14 {
            set raw [expr {($data2 << 7) | $data1}]
            set b [format %.3f [expr {($raw - 8192.0) / 8192.0}]]
            set msg $::pitch_bend_transform_string
        }
        default return
    }

    lassign [apply_custom_transform $c $data1 $v $b $raw] c data1 v b raw

    # Safe substitution - only replace variables that exist
    regsub -all {\$c} $msg $c msg
    regsub -all {\$n} $msg $data1 msg
    regsub -all {\$v} $msg $v msg
    regsub -all {\$b} $msg $b msg
    regsub -all {\$B} $msg $raw msg

    wire $msg
}

# ------------------------------------------------------------
# Exit override
# ------------------------------------------------------------
rename exit ::real_exit
proc exit {{code 0}} {
    cleanup
    ::real_exit $code
}

# ------------------------------------------------------------
# Start
# ------------------------------------------------------------
start_skmidi_pipe
wm deiconify .monitor
wm geometry .monitor 900x900
vwait forever