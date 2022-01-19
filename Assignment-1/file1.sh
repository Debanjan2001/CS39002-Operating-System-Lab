NUM=$1
for((i=2;i<=$1;i++)); do
    while(($NUM%i==0));do
        ((NUM=NUM/i))
        echo -n "$i "
    done
done
