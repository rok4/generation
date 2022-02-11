#!/usr/bin/env bash

setup_file() {
    echo "Testing work2cache" >&3
    load 'test_helper/bats-support/load'
    load 'test_helper/bats-assert/load'
    load 'test_helper/bats-file/load'
    DIR="$( cd "$( dirname "$BATS_TEST_FILENAME" )" >/dev/null 2>&1 && pwd )"
    PATH="$DIR:$PATH"
}

@test "Fail because of grid definition" {
    run -255 work2cache /inputs/work2cache/NOTASLAB.tif -c zip -t 150 150 /tmp/test_nok_param.tif
}

@test "Succeed to tile a slab, with conversion" {
    run -0 work2cache /inputs/work2cache/NOTASLAB.tif -c zip -t 50 50 -a uint -b 8 -s 1 /tmp/test_ok_conversion.tif
}

@test "Succeed to tile a slab, cropping data" {
    run -0 work2cache /inputs/work2cache/NOTASLAB.tif -c jpg -t 100 100 /tmp/test_ok_crop.tif -crop
}

teardown_file() {
    rm -f /tmp/test_nok_param.tif
    rm -f /tmp/test_ok_conversion.tif
    rm -f /tmp/test_ok_crop.tif
}