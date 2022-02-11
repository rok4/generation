#!/usr/bin/env bash

setup_file() {
    echo "Testing decimateNtiff" >&3
    load 'test_helper/bats-support/load'
    load 'test_helper/bats-assert/load'
    load 'test_helper/bats-file/load'
    DIR="$( cd "$( dirname "$BATS_TEST_FILENAME" )" >/dev/null 2>&1 && pwd )"
    PATH="$DIR:$PATH"
}

@test "Fail because of not compatible input images" {
    run -255 decimateNtiff -f /inputs/decimateNtiff/nok/conf.txt -n 255,0,0 -c zip
}

@test "Fail because of wrong nodata colour (not enough value)" {
    run -255 decimateNtiff -f /inputs/decimateNtiff/ok/conf.txt -n 255,0 -c zip
}

@test "Succeed to decimate image" {
    run -0 decimateNtiff -f /inputs/decimateNtiff/ok/conf_bg.txt -n 255,0,0 -c zip
}

@test "Succeed to decimate image with conversion" {
    run -0 decimateNtiff -f /inputs/decimateNtiff/ok/conf.txt -n 255 -c zip -a uint -b 8 -s 1
}

teardown_file() {
    rm -f /tmp/test_ok_scan1000_conversion_i.tif
    rm -f /tmp/test_ok_scan1000_conversion_m.tif
    rm -f /tmp/test_ok_scan1000_bg_i.tif
    rm -f /tmp/test_nok_compatible.tif
}