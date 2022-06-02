#!/bin/bash
#
# code derived from driver.sh by David O'Hallaron, for about the author 
# and rights view the head of driver.sh
#
# test_1.sh - This is exxtention to the simple autograder for the Proxy Lab. It does
#     basic spotchecks that determine whether or not the code
#     behaves well in edge casses related to bad request, shorcomings in tiny servers
#     and consistatly avoids data races 
#
#     adapted from David O'Hallaron, Carnegie Mellon University, 2/8/2016
# 
#     usage: ./test_1.sh
# 

# Point values
MAX_SCORE=8

# Various constants
HOME_DIR=`pwd`
PROXY_DIR="./.proxy"
NOPROXY_DIR="./.noproxy"
TIMEOUT=5
MAX_RAND=63000
PORT_START=1024
PORT_MAX=65000
MAX_PORT_TRIES=10

BASIC_LIST="home.html
            csapp.c
            tiny.c
            godzilla.jpg
            tiny"


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
# THEN THE DEFAULT BEHAVIOR crashes the proxy server
# THIS TEST CHECKS to see if the proxy continues to work after a tiny server fails
# TESTED BY sending a request for a file (Cat.jpg) that is so big that the tiny server crashes
# then the test file spins up a second tiny server to check if the proxy is still working 
# even after one of the servers which the proxy is communicating with goes down
echo ""
echo "*** Checking to make sure a broken tiny server doesn't crash the proxy  ***"


# copy Cat.jpg into the tiny/ directory so there is a resource too large to tiny to work with
# (one of the resorces we submited for grading is a 25mb file called Cat.jpg
# like all of the files we submit, it can be placed into the sime folder as proxy.c)
# here we make a copy of Cat.jpg to the correct place so the tiny server can easily acsess it
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
    cacheScore=${MAX_SCORE}
    echo "Success: proxy was able to gracfully notify the user that they requested too large of a file without crashing"
else
    cacheScore=0
    echo "Failure: requesting too large of a file caused the proxy to crash"
fi


# Kill Tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null



# Kill the proxy
echo "Killing proxy"
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

echo "Response to a broken tiny server score: $cacheScore/8"
echo
echo



####
# Fact: if the user doesn't specify the protical proxy should assume they want http
# Fact: by defualt if the protical is not specified the proxy breaks and sends a bad request
# THIS TEST CHECKS to see if the proxy can catch when no protical is specified and send a valid request
# TESTED BY sending a request which doesn't conatain http then a second, standard request and compairing the two 
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
    cacheScore=${MAX_SCORE}
    echo "Success: proxy was able to add an http specifier to its request when the client didn't specify"
else
    cacheScore=0
    echo "Failure: proxy was unable to add an http specifier to its request when the client didn't specify"
fi


# Kill Tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null



# Kill the proxy
echo "Killing proxy"
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

echo "Aptly handle request when protocal not specified: $cacheScore/8"
echo
echo



#### 
# Fact: it may be tempting to cache and compair cashes without considering port number
# Fact: this would cause issues because one computer can host multiple servers
# FOR EXAMPLE: the sampe computer might host a personal website at port 1000 and a buisness site on port 1100
#               in this example we wouldn't want the localhost:1000/home.html to return the same content as 
#               localhost:1100/home.html thus ports must be considered when testing cashing
# THIS TEST CHECKS to make sure the same requests to different ports do not return the same values
# THIS IS ACCOPLISHED BY starting a single tiny server, making a request, then making the same request at another port
#               this should fail because the cashe shouldn't be triggered and there is no server at this seccond port
#               thus this test makes sure that the program correctly fails
# TESTED BY sending a request which doesn't conatain http then a second, standard request and compairing the two 
echo ""
echo "*** Checking to make sure caching correccty differentuates between ports  ***"



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

# Make a request to proxy
clear_dirs

# requesting godzilla to addit to the cash
download_proxy $PROXY_DIR "godzilla.jpg" "localhost:${tiny_port}/godzilla.jpg" "http://localhost:${proxy_port}" 


#requesting the same addres at the same domain but different port
unused_port=$(free_port)

download_proxy $PROXY_DIR "godzilla.jpg" "http://localhost:${unused_port}/godzilla.jpg" "http://localhost:${proxy_port}" 
# proxy should now be down (unable to fufuill request)

#get control
download_proxy $NOPROXY_DIR "godzilla.jpg" "http://localhost:${tiny_port}/godzilla.jpg" "http://localhost:${proxy_port}" 


# See if the proxy fetch succeeded by comparing it with the original
# file in the tiny directory
diff -q ./tiny/godzilla.jpg ${NOPROXY_DIR}/godzilla.jpg  &> /dev/null
if [ $? -eq 0 ]; then
    cacheScore=0
    echo "Failure: proxy incorrectly found a cashed item dispite request being at a different port"

else
    cacheScore=${MAX_SCORE}
    echo "Success: proxy was able to correctly identify that second request wasn't in the cashe"

fi


# Kill Tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null



# Kill the proxy
echo "Killing proxy"
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

echo "Correctly differentuated between caches of simlar requests: $cacheScore/8"
echo
echo




#### 
# Fact: when we first got multithreading working, we were getting a datarace
# Fact: the way we initialy fixed it is by putting arbirary code at the start of each thread which happened
#       to get the threads far enough out of sink to consistnatly earn a perfect score
# THIS TEST CHECKS to make sure dataraces aren't being avoided by chance by throwing a large numer
#                  of similar request at the same proxy to garandee a datarace would be evident if one was present

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
clear_dirs

echo "Trying to fetch multiple files at the same time from the tiny server."
for i in {1...24}
do
download_proxy $PROXY_DIR "godzilla.jpg" "http://localhost:${tiny_port}/godzilla.jpg" "http://localhost:${proxy_port}" 
download_proxy $PROXY_DIR "csapp.c" "http://localhost:${tiny_port}/csapp.c" "http://localhost:${proxy_port}" 
done


echo "Retrieving and comparing the files to ensure the proxy hasn't broken"
download_noproxy $NOPROXY_DIR "csapp.c" "http://localhost:${tiny_port}/csapp.c" "http://localhost:${proxy_port}" 
# See if the proxy fetch succeeded by comparing it with the original
# file in the tiny directory
diff -q ./tiny/csapp.c ${NOPROXY_DIR}/csapp.c  &> /dev/null
if [ $? -eq 0 ]; then
    download_noproxy $NOPROXY_DIR "godzilla.jpg" "http://localhost:${tiny_port}/godzilla.jpg" "http://localhost:${proxy_port}" 
    diff -q ./tiny/godzilla.jpg ${NOPROXY_DIR}/godzilla.jpg  &> /dev/null
    if [ $? -eq 0 ]; then
        echo "Success - proxy still working after possible race condition: 8/8"
    else
        cacheScore=0
        echo "Failure - proxy not working after possible race condition: 0/8"
    fi
else
    cacheScore=0
    echo "Failure - proxy not working after possible race condition: 0/8"
fi

exit