#!/bin/bash
#
# driver.sh - This is a simple autograder for the Proxy Lab. It does
#     basic sanity checks that determine whether or not the code
#     behaves like a concurrent caching proxy. 
#
#     David O'Hallaron, Carnegie Mellon University
#     updated: 2/8/2016
# 
#     usage: ./driver.sh
# 

# Point values
MAX_BASIC=40
MAX_CONCURRENCY=15
MAX_CACHE=15

# Various constants
HOME_DIR=`pwd`
PROXY_DIR="./.proxy"
NOPROXY_DIR="./.noproxy"
TIMEOUT=5
MAX_RAND=63000
PORT_START=1024
PORT_MAX=65000
MAX_PORT_TRIES=10


#####
# Helper functions
#

#
# download_proxy - download a file from the origin server via the proxy
# usage: download_proxy <testdir> <filename> <origin_url> <proxy_url>
#
function download_proxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --proxy $4 --output $2 $3
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

#
# download_noproxy - download a file directly from the origin server
# usage: download_noproxy <testdir> <filename> <origin_url>
#
function download_noproxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --output $2 $3 
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

#
# clear_dirs - Clear the download directories
#
function clear_dirs {
    rm -rf ${PROXY_DIR}/*
    rm -rf ${NOPROXY_DIR}/*
}

#
# wait_for_port_use - Spins until the TCP port number passed as an
#     argument is actually being used. Times out after 5 seconds.
#
function wait_for_port_use() {
    timeout_count="0"
    if [[ $OSTYPE == 'darwin'* ]]; then
        portsinuse=`netstat -a -n | grep tcp4 | cut -c22- | cut -d' ' -f1 \
            | rev | cut -d'.' -f1 | rev | grep -E "[0-9]+" | uniq | tr "\n" " "`
    else
        portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
            | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
            | grep -E "[0-9]+" | uniq | tr "\n" " "`
    fi

    echo "${portsinuse}" | grep -wq "${1}"
    while [ "$?" != "0" ]
    do
        timeout_count=`expr ${timeout_count} + 1`
        if [ "${timeout_count}" == "${MAX_PORT_TRIES}" ]; then
            kill -ALRM $$
        fi

        sleep 1
            if [[ $OSTYPE == 'darwin'* ]]; then
                portsinuse=`netstat -a -n | grep tcp4 | cut -c22- | cut -d' ' -f1 \
                    | rev | cut -d'.' -f1 | rev | grep -E "[0-9]+" | uniq | tr "\n" " "`
            else
                portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
                    | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
                    | grep -E "[0-9]+" | uniq | tr "\n" " "`
            fi
        echo "${portsinuse}" | grep -wq "${1}"
    done
}


#
# free_port - returns an available unused TCP port 
#
function free_port {
    # Generate a random port in the range [PORT_START,
    # PORT_START+MAX_RAND]. This is needed to avoid collisions when many
    # students are running the driver on the same machine.
    port=$((( RANDOM % ${MAX_RAND}) + ${PORT_START}))

    while [ TRUE ] 
    do
            if [[ $OSTYPE == 'darwin'* ]]; then
                portsinuse=`netstat -a -n | grep tcp4 | cut -c22- | cut -d' ' -f1 \
                    | rev | cut -d'.' -f1 | rev | grep -E "[0-9]+" | uniq | tr "\n" " "`
            else
                portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
                    | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
                    | grep -E "[0-9]+" | uniq | tr "\n" " "`
            fi

        echo "${portsinuse}" | grep -wq "${port}"
        if [ "$?" == "0" ]; then
            if [ $port -eq ${PORT_MAX} ]
            then
                echo "-1"
                return
            fi
            port=`expr ${port} + 1`
        else
            echo "${port}"
            return
        fi
    done
}


#######
# Main 
#######

######
# Verify that we have all of the expected files with the right
# permissions
#

# Kill any stray proxies or tiny servers owned by this user
killall -q proxy tiny nop-server.py 2> /dev/null

# Make sure we have a Tiny directory
if [ ! -d ./tiny ]
then 
    echo "Error: ./tiny directory not found."
    exit
fi

# If there is no Tiny executable, then try to build it
if [ ! -x ./tiny/tiny ]
then 
    echo "Building the tiny executable."
    (cd ./tiny; make)
    echo ""
fi

# Make sure we have all the Tiny files we need
if [ ! -x ./tiny/tiny ]
then 
    echo "Error: ./tiny/tiny not found or not an executable file."
    exit
fi
for file in ${BASIC_LIST}
do
    if [ ! -e ./tiny/${file} ]
    then
        echo "Error: ./tiny/${file} not found."
        exit
    fi
done

# Make sure we have an existing executable proxy
if [ ! -x ./proxy ]
then 
    echo "Error: ./proxy not found or not an executable file. Please rebuild your proxy and try again."
    exit
fi

# Make sure we have an existing executable nop-server.py file
if [ ! -x ./nop-server.py ]
then 
    echo "Error: ./nop-server.py not found or not an executable file."
    exit
fi

# Create the test directories if needed
if [ ! -d ${PROXY_DIR} ]
then
    mkdir ${PROXY_DIR}
fi

if [ ! -d ${NOPROXY_DIR} ]
then
    mkdir ${NOPROXY_DIR}
fi

# Add a handler to generate a meaningful timeout message
trap 'echo "Timeout waiting for the server to grab the port reserved for it"; kill $$' ALRM


####
# Fact: tiny.c web server can only handle small files
# Fact: one proxy.c server can communicate with many tiny.c servers
# IF proxy.c sends a request to a tiny server that break the tiny server
# THEN THE DEFAULT BEHAVIOR crashes the proxy 
# THIS TEST CHECKS to see if the proxy continues to work after a tiny server fails
# TESTED BY sending a request to a file (Cat.jpg) that is so big that the tiny server crashes
# then the test file spins up a second tiny server to check if the proxy is still working 
# even after one of the servers which the proxy is communicating with goes down
echo ""
echo "*** Checking to make sure a broken tiny server doesn't crash the proxy  ***"


# copy Cat.jpg into the tiny/ directory so there is a resource too large to tiny to work with
cp Cat.jpg tiny/Cat.jpg


# Run the Tiny Web server
tiny_port=$(free_port)
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> /dev/null &
tiny_pid=$!
cd ${HOME_DIR}

# Wait for tiny to start in earnest
wait_for_port_use "${tiny_port}"

# Run the proxy
proxy_port=$(free_port)
echo "Starting proxy on port ${proxy_port}"
./proxy ${proxy_port} &> /dev/null &
proxy_pid=$!

# Wait for the proxy to start in earnest
wait_for_port_use "${proxy_port}"

# Fetch some files from tiny using the proxy
clear_dirs
echo "Fetching ./tiny/Cat.jpg into ${PROXY_DIR} using the proxy"
download_proxy $PROXY_DIR "Cat.jpg" "http://localhost:${tiny_port}/Cat.jpg" "http://localhost:${proxy_port}" 

download_proxy $NOPROXY_DIR ${FETCH_FILE} "http://localhost:${tiny_port}/${FETCH_FILE}" "http://localhost:${proxy_port}"

# Re-Run the Tiny Web server
tiny_port=$(free_port)
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> /dev/null &
tiny_pid=$!
cd ${HOME_DIR}

# Wait for tiny to start in earnest
wait_for_port_use "${tiny_port}"

download_proxy $PROXY_DIR "csapp.c" "http://localhost:${tiny_port}/csapp.c" "http://localhost:${proxy_port}"
download_proxy $NOPROXY_DIR "csapp.c" "http://localhost:${tiny_port}/csapp.c" "http://localhost:${proxy_port}"

# See if the proxy fetch succeeded by comparing it with the original
# file in the tiny directory
diff -q ./tiny/csapp.c ${NOPROXY_DIR}/csapp.c  &> /dev/null
if [ $? -eq 0 ]; then
    cacheScore=${MAX_CACHE}
    echo "Success: proxy was able to gracfully notify the user that they requested too large of a file without crashing"
else
    cacheScore=0
    echo "Failure: requesting too large of a file caused the proxy to crash"
fi

# for file in ${CACHE_LIST}
# do
#     echo "Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
#     download_proxy $PROXY_DIR ${file} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
# done

# Kill Tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null



# Kill the proxy
echo "Killing proxy"
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

echo "Response to a broken tiny server score: $cacheScore/15"



####
# Fact: if the user doesn't specify the protical proxy should assume they want http
# Fact: by defualt if the protical is not specified the proxy breaks and sends a bad request
# THIS TEST CHECKS to see if the proxy can catch when no protical is specified and send a valid request
# TESTED BY sending a request which doesn't conatain http then second a second standard request and compairing the two 
echo ""
echo "*** Checking to make sure bad request from a client doesn't break the proxy server  ***"



# Run the Tiny Web server
tiny_port=$(free_port)
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> /dev/null &
tiny_pid=$!
cd ${HOME_DIR}

# Wait for tiny to start in earnest
wait_for_port_use "${tiny_port}"

# Run the proxy
proxy_port=$(free_port)
echo "Starting proxy on port ${proxy_port}"
./proxy ${proxy_port} &> /dev/null &
proxy_pid=$!

# Wait for the proxy to start in earnest
wait_for_port_use "${proxy_port}"

# Make a invalid request to proxy
clear_dirs
echo "asking proxy to request an asset from an unused port (no server is listening on this port)"
download_proxy $PROXY_DIR "godzilla.jpg" "http://localhost:${tiny_port}/godzilla.jpg" "http://localhost:${proxy_port}" 
download_proxy $NOPROXY_DIR "godzilla.jpg" "localhost:${tiny_port}/godzilla.jpg" "http://localhost:${proxy_port}" 
download_proxy $NOPROXY_DIR "godzilla.jpg" "http://localhost:${tiny_port}/godzilla.jpg" "http://localhost:${proxy_port}" 



# See if the proxy fetch succeeded by comparing it with the original
# file in the tiny directory
diff -q ./tiny/godzilla.jpg ${NOPROXY_DIR}/godzilla.jpg  &> /dev/null
if [ $? -eq 0 ]; then
    cacheScore=${MAX_CACHE}
    echo "Success: proxy was able to add an http specifier to its request when the client didn't specify"
else
    cacheScore=0
    echo "Failure: proxy was unable to add an http specifier to its request when the client didn't specify"
fi

# for file in ${CACHE_LIST}
# do
#     echo "Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
#     download_proxy $PROXY_DIR ${file} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
# done

# Kill Tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null



# Kill the proxy
echo "Killing proxy"
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

echo "appliity handle request when protical not specified score: $cacheScore/15"

