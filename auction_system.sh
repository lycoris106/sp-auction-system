#!/bin/bash

M=$1
N=$2

fifo0="fifo_0.tmp"
comb="player_comb.tmp"


function finish {
    rm -f $fifo0
    for i in $(seq 1 $M);
    do
	rm -f "fifo_${i}.tmp"
    done
    rm -f $comb
}

#para1: index, para2: element
function find_combination {
    if [ $1 == 8 ]
    then
	for i in $(seq 0 6);
	do 
	    local tmp3=$((${one_comb[$i]}+1))
	    printf "$tmp3 " >> $comb
	done
	local tmp4=$((${one_comb[7]}+1))
	printf "$tmp4\n" >> $comb
	comb_num=$((comb_num+1))
	return 0
    fi

    if [ $2 -ge $N ]
    then
	return 0
    fi

    one_comb[$1]=$2
    local tmp1=$(($1+1))
    local tmp2=$(($2+1))
    find_combination $tmp1 $tmp2
    find_combination $1 $tmp2

    return 0

}	    
	    


if [ ! -p $fifo0 ]
then
    mkfifo "$fifo0" -m600
fi


#exec 3< $fifo0

for i in {1..32768};
do
    key_map[$i]=-1
done

for i in $(seq 1 $M);
do
    mkfifo fifo_${i}.tmp -m600
    rand=$RANDOM
    while [[ key_map[$((rand+1))] -gt -1 ]]
    do
	rand=$RANDOM
    done
    r=$((rand+1))
    key_map[$r]=$i
    ./host $i $r 0 &
done

printf "1\n"
for i in $(seq 4 $(($1+3)));
do	
    name="fifo_$((i-3)).tmp"
    exec $i> $name
    printf "t\n"
done

exec 3< $fifo0

printf "here\n"

touch $comb
comb_num=0
find_combination 0 0



wait

trap finish EXIT

