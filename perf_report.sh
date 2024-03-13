#!/bin/bash

for cmd_file in ./perf_test/test*.cmd
do
#     echo "${cmd_file%.cmd}"_report
    outputfile=$(echo "${cmd_file}" | sed 's/perf_test/perf_report/')
#    echo "${outputfile%.cmd}"_report 
    perf stat --repeat 5 -o "${outputfile%.cmd}"_report -e cache-misses,branches,instructions,context-switches,cache-references,cycles ./qtest  -f ${cmd_file}
done



