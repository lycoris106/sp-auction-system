#!/bin/bash

M=$1
N=$2

fifo0="fifo_0.tmp"
comb="player_comb.tmp"
neg=-1


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

for i in $(seq 5 $(($M+4)));
do
    name=fifo_$((i-4)).tmp
    eval exec "$i"'>"$name"'
done

exec 3< $fifo0

touch $comb
comb_num=0
find_combination 0 0
#2
for i in $(seq 1 $N);
do
    score[$i]=0
done

sent_num=0
finish_num=0

exec 4< $comb

for i in $(seq 1 $M);
do
    if [ $sent_num -lt $comb_num ]
    then
        if read -u 4 line; then
            printf "$line\n" >> "fifo_${i}.tmp"
            sent_num=$((sent_num+1))
        else
            printf "read error\n"
        fi
    else
        printf "%d -1 -1 -1 -1 -1 -1 -1\n" ${neg} >> "fifo_${i}.tmp"
    fi
done
while [ $finish_num -lt $comb_num ]
do
    read -u 3 line
    key=${line}
    for i in {1..8};
    do
        read -u 3 line
        arr=(${line//[^0-9 ]/})
        ori=score[${arr[0]}]
        rk=${arr[1]}
        score[${arr[0]}]=$((ori+8-rk))
    done
    finish_num=$((finish_num+1))
    id=${key_map[$key]}

    if [ $sent_num -lt $comb_num ]
    then
        if read -u 4 line; then
            printf "$line\n" >> "fifo_${id}.tmp"
            sent_num=$((sent_num+1))
        else
            printf "read error\n"
        fi
    else
        printf "%d -1 -1 -1 -1 -1 -1 -1\n" ${neg} >> "fifo_${id}.tmp"
    fi
done


for i in $(seq 1 $N);
do
    printf "%d %d\n" $i ${score[$i]}
done

wait

trap finish EXIT

