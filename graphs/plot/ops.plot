cyan = "#99ffff"; blue = "#4671d5"; red = "#ff0000"; orange = "#f36e00"
set terminal svg enhanced size 600,400
set output "/home/cg3/hare/build/graphs/ops.svg"
set decimal locale
set style data histograms
set style histogram cluster gap 1
set style histogram rowstacked
set style fill solid border rgb "black"
set key out cent right invert reverse Left font ",11"
set key samplen 0.5 spacing 0.75 width 1 height 0 
set boxwidth 0.75 absolute
#set title "{/=14 Operations}"
set yrange [0:100]
set xrange [0:*]
set ylabel "Operation %"
unset ytics
set xtics rotate by -45 offset character -2, -0.5
plot 'graphs/data/ops.data' using (100.*$2/$14):xticlabels(1) t "CREATE" lc rgb 'red', \
 'graphs/data/ops.data' using (100.*$3/$14):xticlabels(1) t "ADD-MAP" lc rgb 'dark-blue', \
 'graphs/data/ops.data' using (100.*$4/$14):xticlabels(1) t "RM-MAP" lc rgb 'green', \
 'graphs/data/ops.data' using (100.*$5/$14):xticlabels(1) t "OPEN" lc rgb 'yellow', \
 'graphs/data/ops.data' using (100.*$6/$14):xticlabels(1) t "READ" lc rgb 'black', \
 'graphs/data/ops.data' using (100.*$7/$14):xticlabels(1) t "WRITE" lc rgb 'blue', \
 'graphs/data/ops.data' using (100.*$8/$14):xticlabels(1) t "CLOSE" lc rgb 'purple', \
 'graphs/data/ops.data' using (100.*$9/$14):xticlabels(1) t "FSTAT" lc rgb 'pink', \
 'graphs/data/ops.data' using (100.*$10/$14):xticlabels(1) t "DIRENT" lc rgb 'dark-red', \
 'graphs/data/ops.data' using (100.*$11/$14):xticlabels(1) t "UNLINK" lc rgb 'orange', \
 'graphs/data/ops.data' using (100.*$12/$14):xticlabels(1) t "RMDIR" lc rgb 'cyan', \
 'graphs/data/ops.data' using (100.*$13/$14):xticlabels(1) t "RESOLVE" lc rgb 'dark-green'
