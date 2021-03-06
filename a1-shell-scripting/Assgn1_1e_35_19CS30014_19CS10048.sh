log (){
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

log "Setting env_variable REQ_HEADERS..."
export REQ_HEADERS="Accept,Connection"

log "GET-ting example.com..."
curl -s --get https://example.com --output example.html

log "GET-ting IP,Response_Headers..."
curl -s -i --get http://ip.jsontest.com/ 

log "GET-ting Headers..."
FETCHED_HEADERS=`curl -s --get http://headers.jsontest.com/`

log "Checking REQ_HEADERS..."
for HEADER in ${REQ_HEADERS//,/ };do
    VALUE=$(echo $FETCHED_HEADERS | jq ".[\"${HEADER}\"]")
    echo "${HEADER}:$VALUE"
done

log "Checking JSONs..."
> valid.txt 
> invalid.txt
for FILE in 1.e.files/*.json;do
    log ">>Validating $FILE..."
    if [ $(curl -s -d "json=`cat $FILE`" -X POST http://validate.jsontest.com/ | jq ".[\"validate\"]") = "true" ];then
        echo $(basename $FILE) >> valid.txt
    else 
        echo $(basename $FILE) >> invalid.txt
    fi
done

sort valid.txt -o valid.txt
sort invalid.txt -o invalid.txt
