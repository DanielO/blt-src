package require BLT

blt::palette create bluegreen -colorformat name -cdata {
    white  green2
} 
blt::scale .s -orient horizontal -markcolor red3 -hide all \
    -show "ticks ticklabels colorbar arrows mark" \
    -tickdirection out -loose yes -min -23.3 -max 43.2 \
    -title fred -axislinewidth 5 -ticklabelcolor grey0 \
    -colorbarthickness 20 -palette bluegreen -rangecolor white \
    -width 5i -tickfont "Arial 7"
blt::table . \
    0,0 .s -fill both

