#!/usr/bin/env bash

setup_file() {
    echo "Testing merge4tiff" >&3
    load 'test_helper/bats-support/load'
    load 'test_helper/bats-assert/load'
    load 'test_helper/bats-file/load'
    DIR="$( cd "$( dirname "$BATS_TEST_FILENAME" )" >/dev/null 2>&1 && pwd )"
    PATH="$DIR:$PATH"
}

@test "Fail because of wrong nodata colour" {
    run -255 merge4tiff -c zip -n 255,255,255 -i1 /inputs/merge4tiff/01.jpg -i2 /inputs/merge4tiff/02.jpg -i3 /inputs/merge4tiff/03.jpg -m3 /inputs/merge4tiff/03.tif -a uint -b 8 -s 4 -io /tmp/test_nok_param.tif
}

@test "Succeed to merge 3 images and a background" {
    run -0 merge4tiff -c zip -n 0,255,0 -i1 /inputs/merge4tiff/01.jpg -i2 /inputs/merge4tiff/02.jpg -i3 /inputs/merge4tiff/03.jpg -ib /inputs/merge4tiff/bg.tif -io /tmp/test_ok_bg.tif
}

@test "Succeed to merge 3 image and convert" {
    run -0 merge4tiff -c zip -n 255,255,255,0 -i1 /inputs/merge4tiff/01.jpg -i2 /inputs/merge4tiff/02.jpg -i3 /inputs/merge4tiff/03.jpg -m3 /inputs/merge4tiff/03m.tif -a uint -b 8 -s 4 -io /tmp/test_ok_conversion.tif
}

@test "Succeed to merge 3 images and generate the mask" {
    run -0 merge4tiff -c zip -n 0,255,0 -i1 /inputs/merge4tiff/01.jpg -i2 /inputs/merge4tiff/02.jpg -i3 /inputs/merge4tiff/03.jpg -m3 /inputs/merge4tiff/03m.tif -io /tmp/test_ok_mask_i.tif -mo /tmp/test_ok_mask_m.tif
}

teardown_file() {
    rm -f /tmp/test_nok_param.tif
    rm -f /tmp/test_ok_bg.tif
    rm -f /tmp/test_ok_conversion.tif
    rm -f /tmp/test_ok_mask_i.tif
    rm -f /tmp/test_ok_mask_m.tif
}