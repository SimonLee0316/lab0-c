#!/bin/bash

for file in ./*
do
    echo $file
    cat $file
    echo "+++++++++++++++++++++++++++++++++++"
done

