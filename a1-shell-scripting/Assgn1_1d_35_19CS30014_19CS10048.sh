mkdir -p files_mod
for FILE in temp/*;do
    nl -w1 $FILE | sed "s/\s/,/g" > "files_mod/$(basename $FILE)"
done