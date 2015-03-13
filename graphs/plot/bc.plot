cyan = "#99ffff"; blue = "#4671d5"; red = "#ff0000"; orange = "#f36e00"
set terminal svg dashed enhanced size 600,400
set output "/home/cg3/hare/build/graphs/bc.svg"
#set title "{/=20 Buffer Cache}"  font ",20" offset character 0, 0.5, 0

set style data histogram
set style histogram cluster gap 1
set rmargin at screen .95

#set auto x
set yrange [0:*]
set ylabel "Normalized Throughput"
set ytics scale 0.5,0 nomirror
set xtics scale 0,0 rotate by -45 offset character -2, -0.5
unset key

#plot "/home/cg3/hare/graphs/data/bc.data" using 2:xtic(1) title col lc rgb 'red', \
#     '' using 3:xtic(1) title col lc rgb 'blue'

#set style line 1 lt 1 lc rgb "#98FF96"
#set style line 2 lt 1 lc rgb "#FF4C4C"
set style line 1 lt 1 lc rgb "dark-grey"
set style line 2 lt 1 lc rgb "dark-grey"
set style line 3 lc rgb "black" lt 2 lw 1.5
set style fill solid 0.75 border rgb "black"
color(x) = x > 0 ? 1:2
plot 1 w lines ls 3,\
	"/home/cg3/hare/build/graphs/bc.data" using  (column(0)):($2/$3):(0.5):(color(($2-$3)/$2)):xtic(1) title col w boxes lc variable

