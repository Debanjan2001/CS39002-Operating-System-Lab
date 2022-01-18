# ITS WORKING BUT TAKING INFINITE TIME, NEED TO CHANGE

mkdir -p 1.c.out
CNT=0


for FILE in $(find data1c -type f); do
    filename=$(basename -- "$FILE")
    extension=${FILE##*.}
    if [ $extension == $FILE ]; then
        mkdir -p "1.c.out/Nil"
        mv $FILE "1.c.out/Nil/$filename"
    else
        mkdir -p "1.c.out/$extension"
        mv $FILE "1.c.out/$extension/$filename"
    fi

    ((CNT=CNT+1))
    if ((CNT%1000==0)); then 
        echo $CNT
    fi
done;

