cyan = "#99ffff"; blue = "#4671d5"; red = "#ff0000"; orange = "#f36e00"
set terminal svg dashed enhanced size 600,400
set output "/home/cg3/hare/build/graphs/overhead.svg"

set style data histogram
set style histogram cluster gap 1
set rmargin at screen .95

set auto x
set yrange [0:*]
set grid y
set ylabel "Normalized Throughput"
set ytics scale 0.5,0 nomirror
set xtics scale 0,0 rotate by -45 offset character -2, -0.5
set key top right
set style fill solid 1 border rgb "black"

plot "/home/cg3/hare/build/graphs/overhead.data" using ($2/$2):xtic(1) title col lc rgb 'red' lt 1, \
     '' using ($2/$3):xtic(1) title col lc rgb 'blue' lt 1, \
     '' using ($2/$4):xtic(1) title col lc rgb 'green' lt 1, \
     '' using ($2/$5):xtic(1) title col lc rgb 'cyan' lt 1

