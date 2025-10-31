#!/usr/bin/env wish
foreach th [ttk::style theme names] {
    ttk::frame .f$th -padding 10
    grid [ttk::label .f$th.l -text $th] -row 0 -column 0
    grid [ttk::scale .f$th.s -orient horizontal -from 0 -to 100] -row 1 -column 0 -sticky ew
    pack .f$th -fill x -padx 5 -pady 5
}
