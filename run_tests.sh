#! /bin/bash
#Script by Charles Shinaver

EXECUTABLE="virtmem"
usage() {
    cat <<EOF
Usage: run_tests.sh
EOF
    exit 1
}

faults_with_algo() {
    output=$(./$EXECUTABLE $1 $2 "$3" "$4")
    printf "%s,%s,%s," \
        $(echo "$output" | grep faults | cut -f3 -d' ') \
        $(echo "$output" | grep reads | cut -f3 -d' ') \
        $(echo "$output" | grep writes | cut -f3 -d' ')
}

faults_with_algo_newline() {
    output=$(./$EXECUTABLE $1 $2 "$3" "$4")
    printf "%s,%s,%s\n" \
        $(echo "$output" | grep faults | cut -f3 -d' ') \
        $(echo "$output" | grep reads | cut -f3 -d' ') \
        $(echo "$output" | grep writes | cut -f3 -d' ')
}



if ! [ -f $EXECUTABLE ]; then
    echo "$EXECUTABLE executable not found"
    echo "Make sure it's compiled"
    exit 1
fi

echo "Test results"
echo "pages,frames,,,,,,,"
echo "sort_faults,sort_reads,sort_writes,scan_faults,scan_reads,scan_writes,focus_faults,focus_reads,focus_writes"
UPPER_BOUND=100
LOWER_BOUND=5
NPAGES=100
for i in $(seq $LOWER_BOUND $UPPER_BOUND); do
    printf "%s,%s,,,,,,,\n" "$NPAGES" "$i"
    for algo in "sort" "scan" "focus"; do
            faults_with_algo $NPAGES $i "rand" "$algo"
            faults_with_algo $NPAGES $i "fifo" "$algo"
            faults_with_algo_newline $NPAGES $i "custom" "$algo"
    done
done
