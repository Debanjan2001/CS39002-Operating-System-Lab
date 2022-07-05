FILENAME=$1;COLNUM=$2;REGEXP=$3
> $FILENAME
for((i=1;i<=150;i++));do
    row=()
    for((j=1;j<10;j++));do
        row+="$RANDOM,"
    done
    row+=$RANDOM
    echo $row >> $FILENAME
done
if [[ $(awk -F"," -v col=$COLNUM '{print $col}' $FILENAME | grep $REGEXP) ]];then
    echo "YES" 
else
    echo "NO"
fi