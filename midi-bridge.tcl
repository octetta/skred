# midi-bridge

package require udp

set addr 127.0.0.1
set port 60440

set sock [udp_open]
fconfigure $sock -buffering none -translation binary

proc dest {addr port} {
  fconfigure $::sock -remote [list $addr $port]
}

dest $addr $port

proc wire {msg} {
  puts "$msg"
  puts -nonewline $::sock $msg
  # puts $msg
}

set pipe [open "|./skmidi" r]
fconfigure $pipe -blocking 0 -buffering line
# fconfigure $pipe -blocking 1 -buffering line

while {true} {
  if {[gets $pipe line] >= 0} {
  	set bytes {}
  	# puts "got -> $line"
  	foreach hex $line { lappend bytes [expr "0x$hex"] }
  	set status [lindex $bytes 0]
    set cmd     [expr {($status & 0xF0) >> 4}] ;# High nibble (Command)
    set channel [expr {($status & 0x0F) + 1}]  ;# Low nibble (Channel 1-16)
  	switch $cmd {
  		9 {
  			set key [lindex $bytes 1]
  			set vel [lindex $bytes 2]
  			# puts "$channel ON $key $vel"
  			wire "v[expr $channel - 1] n$key l1" 
  		}
  		8 {
  			set key [lindex $bytes 1]
  			set vel [lindex $bytes 2]
  			# puts "$channel OFF $key $vel"
  			wire "v[expr $channel - 1] n$key l0"
  		}
  		14 {
  			set d1 [lindex $bytes 1]
  			set d2 [lindex $bytes 2]
  			set val [expr {($d2 << 7) | $d1}]
  			# puts "$channel BND $val"
  		}
  	}
  	# puts "ch:$channel cmd:$cmd"
  }
  if {[eof $pipe]} { break }
}

puts "done"