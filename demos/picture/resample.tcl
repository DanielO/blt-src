#!../src/bltwish

package require BLT

set imgfile ./images/blt98.gif
set src [image create picture -file $imgfile]  

set name [file root [file tail $imgfile]]
set width [image width $src]
set height [image height $src]

option add *BltTkLabel.background white
. configure -bg white
set type gif
set i 0
foreach scale { 1.0 0.8 0.6666666 0.5 0.4 0.3333333 0.3 0.25 0.2 0.15 0.1 } {
    incr i
    set iw [expr int($width * $scale)]
    set ih [expr int($height * $scale)]
    set r [format %6g [expr 100.0 * $scale]]
    image create picture r$i -width $iw -height $ih
    r$i resample $src -filter sinc
    blt::tk::label .header$i -text "${iw}x${ih}" -bg white
    blt::tk::label .footer$i -text "$r%" -bg white
    blt::tk::label .l$i -image r$i -bg white
    blt::table . \
	0,$i .header$i \
	1,$i .l$i \
	2,$i .footer$i
}


