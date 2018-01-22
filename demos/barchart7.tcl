
blt::barchart .b -barmode aligned
set count 0
set trend {}
foreach { numProc p color } {
    10 495.67 red4
    10 966.67 red3
    12 594 green4
    12 1110.33 green3
    14 693 lightblue4
    14 1359.99 lightblue3
    16 841 cyan4
    16 1649.99 cyan3
    18 990 orange4
    18 1974.99 orange3
} {
    .b element create n$count$numProc -label "$numProc core" \
	-x $numProc -y $p \
	-bg $color -relief flat -fg $color
    lappend trend1 $numProc $numProc*49.5
    incr count
    if { $count & 0x1 } {
	lappend trend2 $numProc $p
    } else {
	lappend trend3 $numProc $p
    }
}

.b legend configure -hide yes
.b axis configure y -loose yes -title "price"
.b axis configure x -loose yes -title "cores" -grid 0
.b line create trend1 -label trend1 -data $trend1 -color red3 \
    -pixels 6 -symbol none
.b line create trend2 -label trend2 -data $trend2 -color blue2 \
    -pixels 6 -symbol none
.b line create trend3 -label trend2 -data $trend3 -color blue2 \
    -pixels 6 -symbol none
.b element raise trend2 trend1

blt::table . \
    0,0 .b -fill both
