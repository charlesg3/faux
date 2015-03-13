last_chars=-1
last_percent=0
term_width=`tput cols`
if [ $term_width -gt 61 ]; then
    term_width=61
fi
right_side=6
pbar_width=$(( term_width - right_side ))
pbar_chars=$(( pbar_width - 2 ))

show_progress ()
{
    local progress=$1
    local total=$2
    local percent=$(( 100 * progress / total ))
    local progress_chars=$(( progress * pbar_chars / total ))

    # only print when we have an update
    if [ $pbar_chars -eq $last_chars ] && [ $percent -eq $last_percent ]
    then
        return
    fi
    last_chars=$pbar_chars
    last_percent=$percent

    echo -n "["
    for (( i=1; i<$pbar_chars; i++ )); do
        if [ $i -lt $progress_chars ]
        then
            echo -n "."
        else
            echo -n " "
        fi
    done

    echo -en "]" $percent "%\r"
}

progress_by_line ()
{
    local value=0
    local total=$1
    while read line
    do
        value=$(( value + 1 ));
        show_progress $value $total
    done
}

