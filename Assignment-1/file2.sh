mkdir -p 1.b.files.out
for FILE in 1.b.files/*; do 
    sort -n $FILE > "1.b.files.out/$(basename $FILE)";
done

#----------------------
# declare -A freq
# for FILE in 1.b.files.out/*; do
#     # cat $FILE >> 1.b.out.txt
#     for NUM in $(cat $FILE); do
#         if [ -v freq[$NUM] ];then
#             freq[$NUM] = freq[$NUM] + 1
#         else
#             freq[$NUM] = 0;
#         fi
#     done 
# done
#----------------------

cat 1.b.files.out/*.txt >> temp;
sort -n temp | uniq -c | awk '{print $2,$1}' > 1.b.out.txt
rm temp