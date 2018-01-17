

package require BLT

blt::graph .g

scale .s -from 0 -to 100 -orient horizontal

blt::table . \
    0,0 .g -fill both \
    1,0 .s -fill x

blt::vector x
x set { 
    2.00000e-01 4.00000e-01 6.00000e-01 8.00000e-01 1.00000e+00 
    1.20000e+00 1.40000e+00 1.60000e+00 1.80000e+00 2.00000e+00 
    2.20000e+00 2.40000e+00 2.60000e+00 2.80000e+00 3.00000e+00 
    3.20000e+00 3.40000e+00 3.60000e+00 3.80000e+00 4.00000e+00 
    4.20000e+00 4.40000e+00 4.60000e+00 4.80000e+00 5.00000e+00 
} 

blt::vector y1
y1 set { 
    4.07008e+01 7.95658e+01 1.16585e+02 1.51750e+02 1.85051e+02 
    2.16479e+02 2.46024e+02 2.73676e+02 2.99427e+02 3.23267e+02 
    3.45187e+02 3.65177e+02 3.83228e+02 3.99331e+02 4.13476e+02 
    4.25655e+02 4.35856e+02 4.44073e+02 4.50294e+02 4.54512e+02 
    4.56716e+02 4.57596e+02 4.58448e+02 4.59299e+02 4.60151e+02 
}

blt::vector y2
y2 set { 
    5.14471e-00 2.09373e+01 2.84608e+01 3.40080e+01 3.75691e+01
    3.91345e+01 3.92706e+01 3.93474e+01 3.94242e+01 3.95010e+01 
    3.95778e+01 3.96545e+01 3.97313e+01 3.98081e+01 3.98849e+01 
    3.99617e+01 4.00384e+01 4.01152e+01 4.01920e+01 4.02688e+01 
    4.03455e+01 4.04223e+01 4.04990e+01 4.05758e+01 4.06526e+01 
}

blt::vector y3
y3 set { 
    2.61825e+01 5.04696e+01 7.28517e+01 9.33192e+01 1.11863e+02 
    1.28473e+02 1.43140e+02 1.55854e+02 1.66606e+02 1.75386e+02 
    1.82185e+02 1.86994e+02 1.89802e+02 1.90683e+02 1.91047e+02 
    1.91411e+02 1.91775e+02 1.92139e+02 1.92503e+02 1.92867e+02 
    1.93231e+02 1.93595e+02 1.93958e+02 1.94322e+02 1.94686e+02 
}

set table [blt::datatable create]
$table import csv -file ./data/graph4a.csv
if { [$table row isheader 0] } {
    set labels [$table row values 0]
    $table column labels $labels
    $table row delete 0
}

puts label=[$table column labels]
blt::vector x0
blt::vector y0
blt::vector xs
blt::vector ys

$table export vector x0 x 
.g element create original -x x0 -y y0 -pixels 6 -label original \
    -color green4 -linewidth 0
.g element create simplified -x xs -y ys -pixels 6 -label simplified \
    -color red4 -linewidth 1

proc generate_element { colName } {
    global table

    $table export vector y0  $colName 
    blt::simplify x0 y0 xs ys 0.0000000005
}

generate_element v20
