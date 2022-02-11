#!/usr/bin/env bash

setup_file() {
    echo "Testing mergeNtiff" >&3
    load 'test_helper/bats-support/load'
    load 'test_helper/bats-assert/load'
    load 'test_helper/bats-file/load'
    DIR="$( cd "$( dirname "$BATS_TEST_FILENAME" )" >/dev/null 2>&1 && pwd )"
    PATH="$DIR:$PATH"
}

@test "Fail because of wrong input" {
    run -255 mergeNtiff -f /inputs/mergeNtiff/conf_nok_/inputs.txt -c zip -i lanczos -n 0,0,255
}

@test "Succeed to reproject images into 4326" {
    run -0 mergeNtiff -f /inputs/mergeNtiff/conf_4326.txt -r /inputs/mergeNtiff/ -c zip -i lanczos -n 255,0 -a uint -b 8 -s 2
}

@test "Succeed to reproject and convert input images" {
    run -0 mergeNtiff -f /inputs/mergeNtiff/conf_conversion.txt -c zip -i lanczos -n 255,255,255,0 -a uint -b 8 -s 4
}

@test "Succeed to reproject images using an input mask" {
    run -0 mergeNtiff -f /inputs/mergeNtiff/conf_mask.txt -r /inputs/mergeNtiff/ -c zip -i lanczos -n 0,0,255
}

@test "Succeed to reproject images and apply a style" {
    run -0 mergeNtiff -f /inputs/mergeNtiff/conf_style.txt -p /inputs/mergeNtiff/pente.json -c zip -i lanczos -n -99999
}

teardown_file() {
    rm -f /tmp/test_ok_style.tif
    rm -f /tmp/test_ok_mask_i.tif
    rm -f /tmp/test_ok_mask_m.tif
    rm -f /tmp/test_ok_conversion.tif
    rm -f /tmp/test_ok_4326_i.tif
    rm -f /tmp/test_ok_4326_m.tif
}