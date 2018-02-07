package require BLT

set normalBg [blt::background create linear \
		  -highcolor grey75 -lowcolor grey98 -jitter 3]
blt::palette create bluegreen -colorformat name -cdata {
    white  green2
} 
blt::scale .s -orient vertical \
    -loose no -min -23.3 -max 43.2 \
    -title fred \
    -palette bluegreen  \
    -scale time \
    -height 5i -bg $normalBg \
    -valueangle 90 \
    -resolution 1

blt::table . \
    0,0 .s -fill both

.s bind value <Enter> {
    puts stderr "entered value"
}
.s bind value <Leave> {
    puts stderr "left value"
}

focus .s
