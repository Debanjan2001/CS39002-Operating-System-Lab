mkdir -p 1.c.out

EXTENSIONS=$(find data1c -type f | perl -ne 'print $1 if m/\.([^.\/]+)$/' | sort -u)

for EXT in $EXTENSIONS; do
    mkdir -p "1.c.out/$EXT"
    find data1c/ -type f -name "*.$EXT" -exec mv -t "1.c.out/$EXT/" {} +
    echo "$EXT done"
done;

mkdir -p 1.c.out/Nil
find data1c/ -type f -exec mv -t "1.c.out/Nil/" {} +