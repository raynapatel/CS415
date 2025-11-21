#!/bin/bash

# script relies on functioning input.txt and 
# Makefile to make all parts
# iobound, and cpubound executables

EXECUTABLE1="part1"
EXECUTABLE2="part2"
EXECUTABLE3="part3"
EXECUTABLE4="part4"

TEST_DIR="test_project2"
INPUT_FILE="input.txt"

TIME_1="8"
TIME_2="8"
TIME_3="18"
TIME_4="18"


setup_test_environment() {
    mkdir $TEST_DIR
    cp $1 $TEST_DIR
    cp $INPUT_FILE $TEST_DIR
    cp iobound $TEST_DIR
    cp cpubound $TEST_DIR
}


cleanup_test_environment() {
    rm -drf $TEST_DIR
}

process_valgrind_output() {
    local valgrind_output=$(echo "$1" | awk 'BEGIN{RS="==[0-9]*== Memcheck, a memory error detector"} END{print RS $0}')
    local mem_errors=$(echo "$valgrind_output" | grep 'ERROR SUMMARY:')
    local leaks_detected=$(echo "$valgrind_output" | grep -Eo 'definitely lost: [^0]|indirectly lost: [^0]|possibly lost: [^0]|still reachable: [^0]')

    if echo "$mem_errors" | grep -q "0 errors"; then
        :
    else
        echo "  - Memory errors detected! $mem_errors"
    fi

    if [ -n "$leaks_detected" ]; then
        echo "  - Memory leaks detected!"
    else
        :
    fi
}


test_invalid_command_line_input() {
    echo "=== Testing if $1 takes the correct arguments... ==="

    if [ ! -f "$1" ]; then
        echo "Error: Compilation failed, $1 executable not found."
    fi

    setup_test_environment $1
    cd $TEST_DIR

    valgrind_output=$(valgrind ./$1 -f  2>&1)

    output=$(./$1 -f 2>&1)

    expected_output="Invalid use: incorrect number of parameters
"

    if [ $(echo "$output" | grep -o . | sort | tr -d  "\n") == $(echo "$expected_output" | grep -o . | sort | tr -d  "\n" ) ]; then
        echo "Success: '$1' is taking the -f flag correctly"
    else
        echo "ERROR: Your project should be able to run like so ./$1 -f $INPUT_FILE"

        echo "Expected output is:"

        echo "'$expected_output'"

        echo "Your output is:"
        echo "'$output'"

    fi

    process_valgrind_output "$valgrind_output"

    echo ""

    cd ..
    cleanup_test_environment
}

test_part1() {

    echo "=== Testing if $1 has correct output... ==="

    if [ ! -f "$1" ]; then
        echo "Error: Compilation failed, $1 executable not found."
    fi

    setup_test_environment $1
    cd $TEST_DIR

    valgrind_output=$(valgrind ./$1 -f $INPUT_FILE 2>&1)


    start_time=$(date +%s.%N)
    output=$(./$1 -f $INPUT_FILE 2>&1)
    end_time=$(date +%s.%N)

    duration=$(echo "$end_time - $start_time" | bc)

    if (( $(echo "$duration < $TIME_1" | bc) )); then
        echo "Error: The operation was shorter than $TIME_1 seconds."
        echo "  - iobound and cpubound should take longer"
        echo "  - Note: I run it twice look at test_script file to see why"
    else
        echo "Success: $1 took the right amount of time"
    fi

    expected_output="part1
iobound
input.txt
cpubound
..
.
Execvp: No such file or directory
Process: - Begining calculation.
Process: - Finished.
Process: - Begining to write to file.
Process: - Finished."

    output=$(echo "$output" | sed -E 's/Process: [0-9]+/Process:/g')


    if [ $(echo "$output" | grep -o . | sort | tr -d  "\n") == $(echo "$expected_output" | grep -o . | sort | tr -d  "\n" ) ]; then
        echo "Success: '$1' has the correct output"
    else
        echo "ERROR: Your $1 does not have correct output ./$1 -f $INPUT_FILE"

        echo "Expected output is:"

        echo "'$expected_output'"

        echo "Your output is:"
        echo "'$output'"
    fi

    process_valgrind_output "$valgrind_output"

    echo ""

    cd ..
    cleanup_test_environment
}

test_part2() {

    echo "=== Testing if $1 has correct output... ==="

    if [ ! -f "$1" ]; then
        echo "Error: Compilation failed, $1 executable not found."
    fi

    setup_test_environment $1
    cd $TEST_DIR


    start_time=$(date +%s.%N)
    output=$(./$1 -f $INPUT_FILE 2>&1)
    end_time=$(date +%s.%N)

    valgrind_output=$(valgrind ./$1 -f $INPUT_FILE 2>&1)

    duration=$(echo "$end_time - $start_time" | bc)

    if (( $(echo "$duration < $TIME_2" | bc) )); then
        echo "Error: The operation was shorter than $TIME_2 seconds."
        echo "  - iobound and cpubound should take longer"
        echo "  - Note: I run it twice look at test_script file to see why"
    else
        echo "Success: $1 took the right amount of time"
    fi

    expected_output="part2
iobound
input.txt
cpubound
..
.
Execvp: No such file or directory
Process: - Begining calculation.
Process: - Finished.
Process: - Begining to write to file.
Process: - Finished."

    output=$(echo "$output" | sed -E 's/Process: [0-9]+/Process:/g')

    #echo "$output" > out.txt
    #echo "$expected_output" > valgrindout.txt
    #exit


    if [ $(echo "$output" | grep -o . | sort | tr -d  "\n") == $(echo "$expected_output" | grep -o . | sort | tr -d  "\n" ) ]; then
        echo "Success: '$1' has the correct output"
    else
        echo "ERROR: Your $1 does not have correct output ./$1 -f $INPUT_FILE"

        echo "Expected output is:"

        echo "'$expected_output'"

        echo "Your output is:"
        echo "'$output'"
    fi

    process_valgrind_output "$valgrind_output"

    echo ""

    cd ..
    cleanup_test_environment
}

test_part3() {

    echo "=== Testing if $1 has correct output... ==="

    if [ ! -f "$1" ]; then
        echo "Error: Compilation failed, $1 executable not found."
    fi

    setup_test_environment $1
    cd $TEST_DIR


    start_time=$(date +%s.%N)
    output=$(./$1 -f $INPUT_FILE 2>&1)
    end_time=$(date +%s.%N)

    valgrind_output=$(valgrind ./$1 -f $INPUT_FILE 2>&1)

    duration=$(echo "$end_time - $start_time" | bc)

    if (( $(echo "$duration < $TIME_3" | bc) )); then
        echo "Error: The operation was shorter than $TIME_3 seconds."
        echo "  - iobound and cpubound should take longer"
        echo "  - Note: I run it twice look at test_script file to see why"
    else
        echo "Success: $1 took the right amount of time"
    fi

    expected_output="part3
iobound
input.txt
cpubound
..
.
Execvp: No such file or directory
Process: - Begining calculation.
Process: - Finished.
Process: - Begining to write to file.
Process: - Finished."

    output=$(echo "$output" | sed -E 's/Process: [0-9]+/Process:/g')

    ###echo "$output" > out.txt
    #echo "$expected_output" > valgrindout.txt
    #exit

    if [ $(echo "$output" | grep -o . | sort | tr -d  "\n") == $(echo "$expected_output" | grep -o . | sort | tr -d  "\n" ) ]; then
        echo "Success: '$1' has the correct output"
    else
        echo "ERROR: Your $1 does not have correct output ./$1 -f $INPUT_FILE"

        echo "Expected output is:"

        echo "'$expected_output'"

        echo "Your output is:"
        echo "'$output'"
    fi

    process_valgrind_output "$valgrind_output"

    echo ""

    cd ..
    cleanup_test_environment
}

test_part4() {

    echo "=== Testing if $1 has correct output... ==="

    if [ ! -f "$1" ]; then
        echo "Error: Compilation failed, $1 executable not found."
    fi

    setup_test_environment $1
    cd $TEST_DIR


    start_time=$(date +%s.%N)
    output=$(./$1 -f $INPUT_FILE 2>&1)
    end_time=$(date +%s.%N)

    valgrind_output=$(valgrind ./$1 -f $INPUT_FILE 2>&1)

    duration=$(echo "$end_time - $start_time" | bc)

    if (( $(echo "$duration < $TIME_4" | bc) )); then
        echo "Error: The operation was shorter than $TIME_4 seconds."
        echo "  - iobound and cpubound should take longer"
        echo "  - Note: I run it twice look at test_script file to see why"
    else
        echo "Success: $1 took the right amount of time"
    fi

    expected_output="part3
iobound
input.txt
cpubound
..
.
Execvp: No such file or directory
Process: - Begining calculation.
Process: - Finished.
Process: - Begining to write to file.
Process: - Finished."

    output=$(echo "$output" | sed -E 's/Process: [0-9]+/Process:/g')

    #echo "$output" > out.txt
    #echo "$expected_output" > valgrindout.txt
    #exit

    duplicates=$(echo "$output" | sort | uniq -d)

    if [ -z "$duplicates" ]; then
        echo "Error: No lines like ex. 'PID    utime    stime    time    nice   virt mem'"
    else
        echo "Success: Table detected"
    fi

    process_valgrind_output "$valgrind_output"

    echo ""

    cd ..
    cleanup_test_environment
}

#------------------------------------

make clean
rm -f $EXECUTABLE1 $EXECUTABLE2 $EXECUTABLE3 $EXECUTABLE4
rm -drf $TEST_DIR
make
gcc iobound.c -o iobound
gcc cpubound.c -o cpubound

echo ""

trap 'echo "Received SIGUSR1, ignoring..."' SIGUSR1
trap 'echo "Received SIGUSR2, ignoring..."' SIGUSR2

test_invalid_command_line_input $EXECUTABLE1
test_part1 $EXECUTABLE1
test_part2 $EXECUTABLE2
test_part3 $EXECUTABLE3
test_part4 $EXECUTABLE4

