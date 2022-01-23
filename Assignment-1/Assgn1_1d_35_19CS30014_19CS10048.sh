mkdir -p files_mod
for FILE in data1d/temp/*; do
    nl -w1 $FILE | sed "s/\s/,/g"  > "files_mod/$(basename $FILE)"
done