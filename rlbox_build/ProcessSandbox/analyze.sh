#! /bin/bash

sort () {
    for ((i=0; i <= $((${#arr[@]} - 2)); ++i))
    do
        for ((j=((i + 1)); j <= ((${#arr[@]} - 1)); ++j))
        do
            if [[ ${arr[i]} -gt ${arr[j]} ]]
            then
                # echo $i $j ${arr[i]} ${arr[j]}
                tmp=${arr[i]}
                arr[i]=${arr[j]}
                arr[j]=$tmp         
            fi
        done
    done
}

filename=$1
i=1
field=()
echo $filename
grep "PID: " $filename | while read -r line; 
do 
    field+={$line}
    i=$((i+1))
#   echo $line
done
echo Num items: ${#field[@]}
echo Data: ${field[@]}
