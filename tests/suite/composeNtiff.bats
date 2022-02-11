#!/usr/bin/env bash

setup_file() {
    echo "Testing composeNtiff" >&3
    load 'test_helper/bats-support/load'
    load 'test_helper/bats-assert/load'
    load 'test_helper/bats-file/load'
    DIR="$( cd "$( dirname "$BATS_TEST_FILENAME" )" >/dev/null 2>&1 && pwd )"
    PATH="$DIR:$PATH"
}

@test "Fail because of different dimension" {
    run -255 composeNtiff -c jpg -s /inputs/composeNtiff/nok/ -g 2 2 /tmp/test_nok_differentdimension.tif
}

@test "Fail because of wrong grid size" {
    run -255 composeNtiff -c jpg -s /inputs/composeNtiff/ok/ -g 3 3 /tmp/test_nok_param.tif
}

@test "Succeed to compose image" {
    run -0 composeNtiff -c zip -s /inputs/composeNtiff/ok/ -g 2 2 /tmp/test_ok_scan1000.tif
}

teardown_file() {
    rm -f /tmp/test_nok_differentdimension.tif
    rm -f /tmp/test_nok_param.tif
    rm -f /tmp/test_ok_scan1000.tif
}