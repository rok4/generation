#!/usr/bin/env bash

setup_file() {
    echo "Testing overlayNtiff" >&3
    load 'test_helper/bats-support/load'
    load 'test_helper/bats-assert/load'
    load 'test_helper/bats-file/load'
    DIR="$( cd "$( dirname "$BATS_TEST_FILENAME" )" >/dev/null 2>&1 && pwd )"
    PATH="$DIR:$PATH"
}

@test "Fail because of wrong input" {
    run -255 overlayNtiff -f /inputs/overlayNtiff/conf.txt -m ALPHATOP -s 2 -c zip -p rgb -t 255,255,255 -b 255
}

@test "Succeed to reproject images into 4326" {
    run -0 overlayNtiff -f /inputs/overlayNtiff/conf.txt -m ALPHATOP -s 4 -c zip -p rgb -t 255,255,255 -b 255,0,0,100
}

teardown_file() {
    rm -f /tmp/test_ok_alphatop.tif
}