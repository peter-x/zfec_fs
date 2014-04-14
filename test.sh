#!/bin/bash

set -u
testdir=/tmp/
[ $# -ge 1 ] && testdir="$1"
testdir=$(mktemp -d "$testdir"/zfec_test_XXXX)

encoder_pid=-1
decoder_pid=-1
echo "Testing zfec_fs in $testdir..."

setup() {
    set -e
    # create directories
    mkdir "$testdir/source"
    mkdir "$testdir/shares"
    mkdir "$testdir/shares_copy"
    mkdir "$testdir/target"

    set +e
    # umount stale mounts
    fusermount -u "$testdir/shares" 2>/dev/null
    fusermount -u "$testdir/target" 2>/dev/null
    set -e

    ./zfec_fs.py --help >/dev/null 2>&1
    ./zfec_fs.py -d -o source="$testdir/source" "$testdir/shares" >"$testdir/log_encode" 2>&1 &
    echo "$!" > "$testdir/encoder_pid"
    ./zfec_fs.py -d -o restore=1 -o source="$testdir/shares_copy" "$testdir/target" >"$testdir/log_decode" 2>&1 &
    echo "$!" > "$testdir/decoder_pid"
    mount | grep -q zfec
    sleep .5
    set +e
}

teardown() {
    # umount stale mounts
    fusermount -u "$testdir/shares" 2>/dev/null
    fusermount -u "$testdir/target" 2>/dev/null
    [ -z "$failed" ] && rm -rf "$testdir"
}

logs_on_failed() {
    # umount stale mounts
    sleep 1
    fusermount -u "$testdir/shares"
    fusermount -u "$testdir/target"
    wait $(cat "$testdir/encoder_pid")
    wait $(cat "$testdir/decoder_pid")
    echo "ENCODER logs ======================================================"
    echo cat "$testdir/log_encode"
    cat "$testdir/log_encode"
    echo
    echo "DECODER logs ======================================================"
    echo cat "$testdir/log_decode"
    cat "$testdir/log_decode"
    for test in $failed
    do
        echo "$test logs ===================================================="
        cat "$testdir/log_$test"
    done
    echo "==================================================================="
}


test_1_encode_listdir() {
    touch "$testdir/source/emptyfile"
    for i in 00 01 02 03 04 05 06 07 08 09 0a 0b
    do
        ls "$testdir/shares/$i"
    done
}

test_2_encode_emptyfile() {
    echo -n > "$testdir/source/emptyfile"
    for i in 00 01 02 03 04 05 06 07 08 09 0a 0b
    do
        [ "3 $testdir/shares/$i/emptyfile" =  "$(wc -c "$testdir/shares/$i/emptyfile")" ]
    done
}

test_3_encode_shortfiles() {
    echo -n 1 > "$testdir/source/one"
    echo -n 12 > "$testdir/source/two"
    echo -n 123 > "$testdir/source/three"
    echo -n 1234 > "$testdir/source/four"
    echo -n 12345 > "$testdir/source/five"
    for i in 00 01 02 03 04 05 06 07 08 09 0a 0b
    do
        for j in one two three
        do
            [ "4 $testdir/shares/$i/$j" = "$(wc -c "$testdir/shares/$i/$j")" ]
        done
        for j in four five
        do
            [ "5 $testdir/shares/$i/$j" =  "$(wc -c "$testdir/shares/$i/$j")" ]
        done
    done
}

test_4_encodedecode_shortfiles_full() {
    echo -n 1 > "$testdir/source/one"
    echo -n 1234 > "$testdir/source/four"
    echo -n 12345 > "$testdir/source/five"
    for i in 00 01 02 03 04 05 06 07 08 09 0a 0b
    do
        mkdir "$testdir/shares_copy/x$i"
        cp "$testdir/shares/$i/"{one,four,five} "$testdir/shares_copy/x$i/"
    done
    for f in one four five
    do
        [ "$(cat "$testdir/source/$f")" -eq "$(cat "$testdir/target/$f")" ] || {
            echo "Files $f differ:"
            echo -n "source:"
            cat "$testdir/source/$f"
            echo 
            echo -n "target:"
            cat "$testdir/target/$f"
            echo 
            false
        }
    done
}

test_5_encodedecode_shortfiles_part() {
    echo -n aoeuhodugcduee > "$testdir/source/part"
    for i in 02 03 08
    do
        cp "$testdir/shares/$i/"part "$testdir/shares_copy/x$i/"
    done
    [ "$(cat "$testdir/source/part")" = "$(cat "$testdir/target/part")" ] || {
        echo "Files differ:"
        echo -n "source:"
        cat "$testdir/source/part"
        echo 
        echo -n "target:"
        cat "$testdir/target/part"
        echo 
        false
    }
}

test_6_encodedecode_longfile_part() {
    dd bs=512 count=10240 if=/dev/urandom of="$testdir/source/longfile"
    for i in 01 04 0a
    do
        cp "$testdir/shares/$i/"longfile "$testdir/shares_copy/x$i/"
    done
    if diff "$testdir/source/longfile" "$testdir/target/longfile"
    then
        echo "Files differ."
    fi
}

test_7_encodedecode_directory() {
    mkdir -p "$testdir/shares_copy/g/x/y/z/a/b/c"
    mkdir -p "$testdir/shares_copy/h/x/y/d"
    mkdir -p "$testdir/shares_copy/q/x/y/z/l"
    [ 1 -eq $(ls "$testdir/target/x/" | wc -w) ]
    [ 2 -eq $(ls "$testdir/target/x/y/" | wc -w) ]
    [ 2 -eq $(ls "$testdir/target/x/y/z/" | wc -w) ]
    [ 1 -eq $(ls "$testdir/target/x/y/z/a/" | wc -w) ]
}

echo "Running setup..."
setup || {
  echo "setup FAILED"; logs_on_failed; exit 1
}
echo "DONE"

failed=
# list all tests
tests=$(declare -F | grep '^declare -f test_' | sed -e 's/^declare -f //')
for test in $tests
do
    echo -n "RUNNING $test..."
    if (set -e; $test) > "$testdir/log_$test" 2>&1
    then
        echo "SUCCESS"
    else
        echo "FAILURE"
        failed="$failed $test"
    fi
done

if [ -n "$failed" ]
then
    echo "The following tests FAILED: $failed"
    logs_on_failed
    echo "The following tests FAILED: $failed"
else
    echo "Running teardown..."
    teardown
    echo "DONE"
fi
