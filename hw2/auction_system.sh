#! /usr/bin/env bash

rm fifo_* 2>&-
n_host=$(($1))
n_player=$(($2))

mkfifo fifo_0.tmp 2>&-
exec 3<>fifo_0.tmp 2>&-
i=1
while [[ ${i} -le ${n_host} ]]; do
	mkfifo fifo_"${i}".tmp 2>&-
	eval "exec $((${i}+3))<>fifo_${i}.tmp" 2>&-
	i=$(($i+1))
done

declare -a combination
tmp=(0 0 0 0 0 0 0 0)
tmp[0]=1
total_auction=0
while [[ $((${tmp[0]}+7)) -le ${n_player} ]];do
	tmp[1]=$((${tmp[0]}+1))
	while [[ $((${tmp[1]}+6)) -le ${n_player} ]];do
		tmp[2]=$((${tmp[1]}+1))
		while [[ $((${tmp[2]}+5)) -le ${n_player} ]];do
			tmp[3]=$((${tmp[2]}+1))
			while [[ $((${tmp[3]}+4)) -le ${n_player} ]];do
				tmp[4]=$((${tmp[3]}+1))
				while [[ $((${tmp[4]}+3)) -le ${n_player} ]];do
					tmp[5]=$((${tmp[4]}+1))
					while [[ $((${tmp[5]}+2)) -le ${n_player} ]];do
						tmp[6]=$((${tmp[5]}+1))
						while [[ $((${tmp[6]}+1)) -le ${n_player} ]];do
							tmp[7]=$((${tmp[6]}+1))
							while [[ $((${tmp[7]})) -le ${n_player} ]];do
								combination+=(${tmp[@]})
								total_auction=$(($total_auction+1))
								tmp[7]=$((${tmp[7]}+1))
							done
							tmp[6]=$((${tmp[6]}+1))
						done
						tmp[5]=$((${tmp[5]}+1))
					done
					tmp[4]=$((${tmp[4]}+1))
				done
				tmp[3]=$((${tmp[3]}+1))
			done
			tmp[2]=$((${tmp[2]}+1))
		done
		tmp[1]=$((${tmp[1]}+1))
	done
	tmp[0]=$((${tmp[0]}+1))
done
# note: a combination [cur,cur+8)

declare -A dict_key
declare -a pids # used for wait
i=1
while [[ ${i} -le ${n_host} ]]; do
	key="${RANDOM}" #0-32767
	while [[ "${!dict_key[@]}" =~ "${key}" ]]; do
		key=${RANDOM}
	done
	dict_key+=(["${key}"]="${i}")
	./host "${i}" "${key}" 0 &
	#echo $?
	pids+=($!)
	i=$(($i+1))
done

cur=0
id=1
while [[ ${cur} -lt ${#combination[@]} ]] && [[ ${id} -le ${n_host} ]]; do
	echo "${combination[$cur]} ${combination[$(($cur+1))]} ${combination[$(($cur+2))]} ${combination[$(($cur+3))]}\
	 ${combination[$(($cur+4))]} ${combination[$(($cur+5))]} ${combination[$(($cur+6))]}\
	 ${combination[$(($cur+7))]}" > fifo_"${id}".tmp 2>&-
	#echo "1 2 3 4 5 6 7 8" > "fifo_${id}.tmp"
	cur=$(($cur+8))
	id=$(($id+1))
done

score=(0 7 6 5 4 3 2 1 0)
declare -a player_score=(0)
i=1
while [[ ${i} -le ${n_player} ]]; do
	player_score+=(0)
	i=$(($i+1))
done

ret=0
#echo "total_auction=${total_auction}"
while [[ ${ret} -lt ${total_auction} ]]; do
	#echo "ret=${ret}"
	#cat fifo_0.tmp
	read key < fifo_0.tmp 2>&-
	i=1
	for i in {1..8}; do
		#echo ${i}
		read -d' ' player < fifo_0.tmp 2>&-
		read rk < fifo_0.tmp 2>&-
		player_score[${player}]=$((${player_score[${player}]}+${score[${rk}]}))
		#echo "${player} ${rk}"
	done
	if [[ cur -lt ${#combination[@]} ]]; then
		echo "${combination[$cur]} ${combination[$(($cur+1))]} ${combination[$(($cur+2))]} ${combination[$(($cur+3))]}\
		 ${combination[$(($cur+4))]} ${combination[$(($cur+5))]} ${combination[$(($cur+6))]}\
		 ${combination[$(($cur+7))]}" > fifo_"${dict_key[${key}]}".tmp 2>&-
		cur=$(($cur+8))
	fi
	ret=$(($ret+1))
done

id=1
while [[ ${id} -le ${n_host} ]]; do
	echo "-1 -1 -1 -1 -1 -1 -1 -1" > fifo_"${id}".tmp 2>&-
	id=$(($id+1))
done

#echo "finish"

i=1
while [[ ${i} -le ${n_player} ]]; do
	echo "${i} ${player_score[${i}]}" 2>&-
	i=$(($i+1))
done

rm fifo* 2>&-

for i in ${pids[@]}; do
	wait ${i} 2>&-
done



