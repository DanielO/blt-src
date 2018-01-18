
blt::barchart .b
set trend {}
foreach { numProc price color } {
    10 495 red4
    12 594 green4
    14 693 lightblue4
    16 891 cyan4
    18 990 orange4
} {
    .b element create n$numProc -label "$numProc core" -x $numProc -y $price \
	-bg $color -relief flat -fg $color
    lappend trend1 $numProc $numProc*49.5
    lappend trend2 $numProc $price
}

.b legend configure -hide yes
.b axis configure y -loose yes -title "price"
.b axis configure x -loose yes -title "cores"
.b line create trend1 -label trend1 -data $trend1 -color red3 \
    -pixels 6 -symbol none
.b line create trend2 -label trend2 -data $trend2 -color blue2 \
    -pixels 6 -symbol none
.b element raise trend2 trend1

blt::table . \
    0,0 .b -fill both
