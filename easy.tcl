#!/usr/bin/env tclsh8

package require udp

set addr 127.0.0.1
set port 60440

set sock [udp_open]
fconfigure $sock -buffering none -translation binary
proc dest {addr port} {
    fconfigure $::sock -remote [list $addr $port]
}
dest $addr $port

# fconfigure $sock -remote [list 127.0.0.1 60440]

proc wire {msg} {
    puts -nonewline $::sock $msg
}

package require Tk

wm title . "fire on a wire"
# wm geometry . 800x640
# wm geometry . 400x640
frame .w0 -padx 10 -pady 10

set max 4
for {set i 0} {$i < $max} {incr i} {
    set ::pt$i {}
    button .w0.p$i -text $i -command "wire \$pt$i"
    entry .w0.e$i -width 80 -textvariable pt$i
    grid .w0.p$i -row $i -column 0
    grid .w0.e$i -row $i -column 1
}

set new_addr $addr
set new_port $port

button .w0.c0 -text dest -command {dest $new_addr $new_port}
frame .w0.f0
grid .w0.c0 -row $max -column 0
grid .w0.f0 -row $max -column 1
entry .w0.f0.c1 -width 24 -textvariable new_addr
entry .w0.f0.c2 -width 8 -textvariable new_port
grid .w0.f0.c1 -row 0 -column 0
grid .w0.f0.c2 -row 0 -column 1

button .w0.l -text load -command {loader easy.txt}
grid .w0.l -row [expr $max+1] -column 0

pack .w0

proc loader {file} {
    set f [open $file]
    set i 0
    while {$i < $::max} {
        set line [gets $f]
        if {[eof $f]} {
            close $f
            break
        }
        set ::pt$i $line
        incr i
    }
}

loader easy.txt
