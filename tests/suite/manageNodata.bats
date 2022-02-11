#!/usr/bin/env bash

setup_file() {
    echo "Testing manageNodata" >&3
    load 'test_helper/bats-support/load'
    load 'test_helper/bats-assert/load'
    load 'test_helper/bats-file/load'
    DIR="$( cd "$( dirname "$BATS_TEST_FILENAME" )" >/dev/null 2>&1 && pwd )"
    PATH="$DIR:$PATH"
}

@test "Fail because of wrong data new color" {
    run -255 manageNodata -target 255,255,255 -touch-edges -data 254,254 /inputs/manageNodata/base.tif /tmp/test_nok_param.tif -channels 3 -format uint8
}

@test "Succeed to generate mask from image" {
    run -0 manageNodata -target 255,255,255 /inputs/manageNodata/base.png -mask-out /tmp/test_ok_mask.tif -channels 3 -format uint8
}

@test "Succeed to generate mask from image spreading from edges" {
    run -0 manageNodata -target 255,255,255 -touch-edges /inputs/manageNodata/base.png -mask-out /tmp/test_ok_mask_touchedges.tif -channels 3 -format uint8
}

@test "Succeed to modify data and nodata color in input image" {
    run -0 manageNodata -target 255,255,255 -touch-edges -data 255,0,0 -nodata 200,200,255 /inputs/manageNodata/base.png /tmp/test_ok_data_nodata.tif -channels 3 -format uint8
}

teardown_file() {
    rm -f /tmp/test_nok_param.tif
    rm -f /tmp/test_ok_mask.tif
    rm -f /tmp/test_ok_mask_touchedges.tif
    rm -f /tmp/test_ok_data_nodata.tif
}