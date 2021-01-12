#!/bin/bash

#rm point_adap_exp.csv
rm range_adap_exp.csv

dataset=$1
#./build/adap-exp $dataset point >> point_adap_exp.csv
./build/adap-exp $dataset range >> range_adap_exp.csv
