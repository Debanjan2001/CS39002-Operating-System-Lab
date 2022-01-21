mkdir -p 1.b.files.out
for FILE in 1.b.files/*; do 
    sort -n $FILE > "1.b.files.out/$(basename $FILE)";
done
sort -n 1.b.files.out/*.txt | uniq -c | awk '{print $2,$1}' > 1.b.out.txt