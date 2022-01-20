function log () {
    if [ $VERBOSE -eq 1 ];then  
        echo "$@"
    fi
}

VERBOSE=0
while getopts ":v" OPT;do
   case $OPT in
      v) VERBOSE=1;;
   esac
done

log "Setting env variable REQ_HEADERS..."
export REQ_HEADERS="Accept,Connection"

log "GET-ting example.com to example.html..."
curl -s --get https://example.com --output example.html
echo "example.html file downloaded."

log "GET-ting IP and Response Headers..."
curl -s -i --get http://ip.jsontest.com/ 

log "GET-ting Headers..."
FETCHED_HEADERS=`curl -s --get http://headers.jsontest.com/`
echo $FETCHED_HEADERS

log "Checking for REQ_HEADERS..."
for HEADER in ${REQ_HEADERS//,/ };do
    VALUE=$(echo $FETCHED_HEADERS | jq ".[\"${HEADER}\"]")
    if [ $VALUE != 'null' ];then
        echo "${HEADER}:$VALUE"
    else 
        echo "${HEADER}: not found"
    fi
done

log "Checking validity of JSON files..."
echo -n > ./1.e.files/valid.txt 
echo -n > ./1.e.files/invalid.txt
for FILE in 1.e.files/*.json;do
    log ">>Validating $(basename $FILE)..."
    if [ $(curl -s -d "json=`cat $FILE`" -X POST http://validate.jsontest.com/ | jq ".[\"validate\"]") = "true" ];then
        echo $(basename $FILE) >> ./1.e.files/valid.txt
    else 
        echo $(basename $FILE) >> ./1.e.files/invalid.txt
    fi
done
