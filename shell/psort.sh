#! /bin/bash

MAX_LINES_PER_CHUNK=50000000
ORIGINAL_FILE=$1
SORTED_FILE=$2
PROCESSORS=$3
CHUNK_FILE_PREFIX=$ORIGINAL_FILE.split.
SORTED_CHUNK_FILES=$CHUNK_FILE_PREFIX*.sorted

usage ()
{
    echo Parallel sort
    echo usage: psort file1 file2 count
    echo Sorts text file file1 and stores the output in file2
    echo Note: file1 will be split in chunks up to $MAX_LINES_PER_CHUNK lines
    echo  and each chunk will be sorted in parallel
}

log ()
{
    ts=`date "+%Y:%m:%d %H:%M:%S"`
    echo "<$ts> $1"
}

# test if we have two arguments on the command line
if [ $# != 3 ]
then
    usage
    exit
fi

#Cleanup any lefover files
rm -f $SORTED_CHUNK_FILES > /dev/null
rm -f $CHUNK_FILE_PREFIX* > /dev/null
rm -f $SORTED_FILE

#Splitting $ORIGINAL_FILE into chunks ...
log "split files..."
split -l $MAX_LINES_PER_CHUNK $ORIGINAL_FILE $CHUNK_FILE_PREFIX


counter=0
for file in $CHUNK_FILE_PREFIX*
do
    let "counter += 1"

    export LC_ALL=zh_CN.utf8
    sort -t $'\t' -k2,2 $file -o $file.sorted &

    log "start the job $counter for file $file..." 
    if [[ $(( counter % PROCESSORS )) -eq 0 ]]
    then
        log "counter = $counter,wait..."
        wait
    fi
done

wait

#Merging chunks to $SORTED_FILE ...
log "merge all sorted files..."
export LC_ALL=zh_CN.utf8
sort -t $'\t' -k2,2 -m $SORTED_CHUNK_FILES -o $SORTED_FILE

#Cleanup any lefover files
log "cleaning temporary files..."
rm -f $SORTED_CHUNK_FILES > /dev/null
rm -f $CHUNK_FILE_PREFIX* > /dev/null

log "all job are done!"
