read -p '>> Enter Name of file: ' FILENAME
> $FILENAME
for ((i=1;i<=150;i++));do
    row=()
    for ((j=1;j<10;j++));do
        row+="$RANDOM,"
    done
    row+=$RANDOM
    echo $row >> $FILENAME
done
read -p '>> Enter Column Number(1-10): ' COLNUM
read -p '>> Enter Regular Expression: ' REGEXP
if  [[ $(cut -d"," -f$COLNUM $FILENAME | grep $REGEXP) ]];then
    echo "YES"
else
    echo "NO"
fi




