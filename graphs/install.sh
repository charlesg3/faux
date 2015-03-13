#!/bin/bash
source `dirname $0`/graphlib.sh

echo "installing graphs..."
echo "install dir: " $HARE_INSTALL

graphdir=$HARE_ROOT/graphs
graphbuild=$HARE_BUILD/graphs
graphinstall=$HARE_INSTALL
apps=$1

# copy the graphs to dest
for app in ${apps[@]} ; do
	cp -u $graphbuild/$app.svg $graphinstall
done

function write_text()
{
	text=$@
	echo $text >> $graphinstall/graphs.html
}

# header
cat - > $graphinstall/graphs.html <<EOF_
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Hare Results</title>
  <script type="text/javascript" src="javascript/hare.js"></script>
  <link rel="stylesheet" type="text/css" href="css/hare.css">
  <link rel="stylesheet" type="text/css" href="css/highlight.css">
</head>
<body onload='select_graph("manyfiles");'>
EOF_

write_text "<div class=\"graphs\">"

for app in ${apps[@]} ; do
title=`get_conf $graph_src $app "title"`
desc=`get_conf $graph_src $app "desc"`
filename=`get_conf $graph_src $app "filename"`
generated=`find $graphbuild/$app.svg -printf "%Ch %Cd %CY %Cr\n"`

if [ "$filename" != "" ] ; then
    highlight -f -o $graphinstall/${app}_file.html -i $HARE_ROOT/$filename
    filespan="Source: <a href=\"#\" onclick='toggle_code(\"$app\")'>$filename (click to toggle)</a><br /><span class=\"code\" id=\"${app}.code\"><pre>"
    filespan=${filespan}`cat $graphinstall/${app}_file.html`"</pre></span>"
fi

cat - >> $graphinstall/graphs.html <<EOF_
<div class="ti" onclick='select_graph("$app")'>
<span class="graph_title" id="$app.title">$title</span><br />
<img src="$app.svg" class="graph"></img>
<span class="graph_desc" id="$app.desc">
<b>Notes:</b> $desc<br /><br />
$filespan<br />
Generated: $generated
</span>
</div>
EOF_
done

write_text "</div>"

cat - >> $graphinstall/graphs.html <<EOF_
<div class="page">
<div class="graph_view"><div class="graph_select" id="graph_view"></div></div>
EOF_

# footer
write_text "<div class=\"footer\">page generated: " `date` "</div>"
cat - >> $graphinstall/graphs.html <<EOF_
</div>
</body>
</html>
EOF_

