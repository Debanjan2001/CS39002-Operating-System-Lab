FILENAME=$1
read -p '>> Enter Column Number: ' COLNUM
awk -v col=$COLNUM '{print tolower($col)}' $1 | 
sort | 
uniq -c | 
sort -k 1 -rn |
awk '{print $2,$1}' > 1f_output_${COLNUM}_column.freq
