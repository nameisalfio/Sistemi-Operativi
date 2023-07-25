#!/bin/bash

for ((i=1; i<=10; i++)); do
  echo "$i) Run"
  gcc myfgrep.c -o myfgrep && ./myfgrep -i al nomi.txt cognomi.txt
  echo 
done

