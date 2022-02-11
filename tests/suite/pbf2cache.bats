#!/usr/bin/env bash

setup_file() {
    echo "Testing pbf2cache" >&3
    load 'test_helper/bats-support/load'
    load 'test_helper/bats-assert/load'
    load 'test_helper/bats-file/load'
    DIR="$( cd "$( dirname "$BATS_TEST_FILENAME" )" >/dev/null 2>&1 && pwd )"
    PATH="$DIR:$PATH"
}

@test "Fail because of grid definition" {
    run -255 pbf2cache -r /inputs/pbfs/ -t 3 -ultile 258 175 /tmp/test_nok_param.tif
}

@test "Succeed to package pbf tiles into a slab" {
    run -0 pbf2cache -r /inputs/pbfs/ -t 3 3 -ultile 258 175 /tmp/test_ok_file_full.tif
}

@test "Succeed to package pbf tiles into a slab, with hole" {
    run -0 pbf2cache -r /inputs/pbfs/ -t 5 5 -ultile 257 174 /tmp/test_ok_file_hole.tif
}

teardown_file() {
    rm -f /tmp/test_nok_param.tif
    rm -f /tmp/test_ok_file_full.tif
    rm -f /tmp/test_ok_file_hole.tif
}