#!/bin/bash

gcc lookup_database.c -o lookup_database && ./lookup_database db1.txt db2.txt db3.txt db4.txt db5.txt
rm lookup_database
