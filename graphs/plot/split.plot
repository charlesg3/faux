cyan = "#99ffff"; blue = "#4671d5"; red = "#ff0000"; orange = "#f36e00"
set decimal locale
set auto y
#set style data histogram
#set style histogram cluster gap 1
#set style fill solid border -1
set boxwidth 0.9
set xtic scale 0

set terminal svg enhanced size 600,400
set output "/home/cg3/hare/build/graphs/split.svg"

unset title
#set title "{/=20 Scalability}\n{/=14 Runtime reduces porportional to cores}"  font ",20" offset character 0, 0.5, 0
#set tmargin at screen 0.8
#set bmargin at screen 0.15
set key samplen 1.5 spacing 1 height .5 font ",12" width 0 maxcols 8
set key vert Right box
#set key at screen 0,1 left top
set key right out
set xlabel "cores" offset character 0, 0.5, 0
set xrange [0:41]
set xtics rotate by 0 0,4,40
set xtics add ("2" 2)
set ytics nomirror
set ylabel "Servers"
set format y "%.1s %c"
set grid y
set style data linespoints

plot "/home/cg3/hare/graphs/data/split.data" \
    u 1:( $2 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'orange' ti col \
, ''u 1:( $3 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'purple' ti col \
, ''u 1:( $4 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'blue' ti col \
, ''u 1:( $5 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'red' ti col \
, ''u 1:( $6 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'green' ti col \
, ''u 1:( $7 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'cyan' ti col \
, ''u 1:( $8 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'dark-red' ti col \
, ''u 1:( $9 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'dark-green' ti col \
, ''u 1:( $10 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'dark-yellow' ti col \
, ''u 1:( $11 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'dark-orange' ti col \
, ''u 1:( $12 ) with linesp lt 1 lw 2 pt 7 ps 0.75 lc rgb 'dark-grey' ti col \

