package require BLT

set normalBg [blt::background create linear \
		  -highcolor grey75 -lowcolor grey98 -jitter 3]
blt::palette create bluegreen -colorformat name -cdata {
    white  green2
} 
blt::scale .s -orient horizontal \
    -loose no -min -23.3 -max 43.2 \
    -title fred  -show value -variable fred \
    -palette bluegreen  \
    -width 5i -bg $normalBg -resolution 1.0 -units C 

blt::table . \
    0,0 .s -fill both

focus .s

after 5000 {
    set fred 23.0
}
