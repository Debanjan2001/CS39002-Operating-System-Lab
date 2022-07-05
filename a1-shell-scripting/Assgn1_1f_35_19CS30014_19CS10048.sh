FILENAME=$1;COLNUM=$2
awk -v col=$COLNUM '{print tolower($col)}' $1 | 
sort | 
uniq -c | 
sort -k 1 -rn |
awk '{print $2,$1}' > 1f_output_${COLNUM}_column.freq