
dir="../AMENG"

num=11
base=100

while [ ${num} -lt 100 ]
do
	number=$((base+num))
	from="0${number}.wav"
	to="${num}.wav"

	echo "cp ${dir}/${from} ${to}"
	cp ${dir}/${from} ${to}

	num=$((num+1))
done


