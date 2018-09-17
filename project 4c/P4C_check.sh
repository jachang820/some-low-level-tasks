#!/bin/bash
#
# sanity check script for Project 4C
#	tarball name
#	tarball contents
#	student identification 
#	makefile targets
#	successful make clean
#	successful make dist
#	successful default build
#	unrecognized parameters
#	recognizes standard parameters
#	retrieve TCP/TLS session logs
#	    confirm successful session identification
#	    confirm successful session completion
#	    confirm server validation of all reports
#
# Note: if your program can build on any Linux system
#	(becuase you have dummied the sensor I/O)
#	this script can be run on any Linux system.
#
#
LAB="lab4c"
README="README"
MAKEFILE="Makefile"

EXPECTED=""
EXPECTEDS="c"
PGMS="lab4c_tcp lab4c_tls"

BASE_URL="http://lever.cs.ucla.edu"
SERVERLOG="server.log"
CLIENT_PFX="client_"
CLIENT_SFX="log"

TIMEOUT=5
MIN_REPORTS=15

let errors=0

if [ -z "$1" ]
then
	echo usage: $0 your-student-id
	exit 1
else
	student=$1
fi

# make sure the tarball has the right name
tarball="$LAB-$student.tar.gz"
if [ ! -s $tarball ]
then
	echo "ERROR: Unable to find submission tarball:" $tarball
	exit 1
fi

# make sure we can untar it
TEMP="/tmp/TestTemp.$$"
echo "... Using temporary testing directory" $TEMP
function cleanup {
	cd
	rm -rf $TEMP
	exit $1
}

mkdir $TEMP
cp $tarball $TEMP
cd $TEMP
echo "... untaring" $tarbsll
tar xvf $tarball
if [ $? -ne 0 ]
then
	echo "ERROR: Error untarring $tarball"
	cleanup 1
fi

# make sure we find all the expected files
echo "... checking for expected files"
for i in $README $MAKEFILE $EXPECTED
do
	if [ ! -s $i ]
	then
		echo "ERROR: unable to find file" $i
		let errors+=1
	else
		echo "        $i ... OK"
	fi
done

# make sure the README contains name and e-mail
echo "... checking for submitter info in $README"
function idString {
	result=`grep $1 $README | cut -d: -f2 | tr -d \[:blank:\] | tr -d "\r"`
	if [ -z "$result" ]
	then
		echo "ERROR - $README contains no $1";
		let errors+=1
	elif [ -z "$2" ]
	then
		# no match required
		echo "        $1 ... $result"
	else
		f1=`echo $result | cut -f1 -d,`
		f2=`echo $result | cut -f2 -d,`
		if [ "$f1" == "$2" ]
		then
			echo "        $1 ... $f1"
		elif [ -n "$f2" -a "$2" == "$f2" ]
		then
			echo "        $1 ... $f1,$f2"
		else
			echo "ERROR: $1 does not include $2"
			let errors+=1
		fi
	fi
}

idString "NAME:"
idString "EMAIL:"
idString "ID:" $student

function makeTarget {
	result=`grep $1: $MAKEFILE`
	if [ $? -ne 0 ]
	then
		echo "ERROR: no $1 target in $MAKEFILE"
		let errors+=1
	else
		echo "        $1 ... OK"
	fi
}

echo "... checking for expected make targets"
makeTarget "clean"
makeTarget "dist"

# make sure we find files with all the expected suffixes
echo "... checking for other files of expected types"
for s in $EXPECTEDS
do
	names=`echo *.$s`
	if [ "$names" = '*'.$s ]
	then
		echo "ERROR: unable to find any .$s files"
		let errors+=1
	else
		for f in $names
		do
			echo "        $f ... OK"
		done
	fi
done

# make sure we can build the expected program
echo "... building default target(s)"
make 2> STDERR
RET=$?
if [ $RET -ne 0 ]
then
	echo "ERROR: default make fails RC=$RET"
	let errors+=1
fi
if [ -s STDERR ]
then
	echo "ERROR: make produced output to stderr"
	let errors+=1
fi

# check a make clean (successful, deletes proeucts)
echo "... testing make clean"
make clean 2> STDERR
RET=$?
if [ $RET -ne 0 ]
then
	echo "ERROR: make clean fails RC=$RET"
	let errors+=1
fi
if [ -s STDERR ]
then
	echo "ERROR: make clean produced output to stderr"
	let errors+=1
fi

# check if a make clean eliminated all targets
for t in $PGMS $tarball
do
	if [ -s $t ]
	then
		echo "ERROR: make clean leaves $t"
		let errors+=1
	fi
done
for t in *.o *.dSYM
do
	if [ -s $t ]
	then
		echo "ERROR: make clean leaves $t"
		let errors+=1
	fi
done

# check if a make dist succeeds
echo "... testing make dist"
make dist 2> STDERR
RET=$?
if [ $RET -ne 0 ]
then
	echo "ERROR: make dist fails RC=$RET"
	let errors+=1
fi
if [ -s STDERR ]
then
	echo "ERROR: make dist produced output to stderr"
	let errors+=1
fi

# check if a make dist creates the tarball
if [ ! -s $tarball ]
then
	echo "ERROR: make dist does not produce $tarball"
	let errors+=1
fi

# check a make after a make clean
echo "... re-building default target(s)"
make 2> STDERR
RET=$?
if [ $RET -ne 0 ]
then
	echo "ERROR: default make fails RC=$RET"
	let errors+=1
fi
if [ -s STDERR ]
then
	echo "ERROR: make produced output to stderr"
	let errors+=1
fi

echo "... checking for expected products"
for p in $PGMS
do
	if [ ! -x $p ]
	then
		echo "ERROR: unable to find expected executable" $p
		let errors+=1
	else
		echo "        $p ... OK"
	fi
done

# see if it accepts the expected arguments
function testrc {
	if [ $1 -ne $2 ]
	then
		echo "ERROR: expected RC=$2, GOT $1"
		let errors+=1
	fi
}

# see if they detect and report invalid arguments
for p in $PGMS
do
	echo "... $p detects/reports bogus arguments"
	if [ -x $p ]
	then
		./$p --bogus > /dev/null 2>STDERR
		testrc $? 1
		if [ ! -s STDERR ]
		then
			echo "ERROR: No Usage message to stderr for --bogus"
			let errors+=1
		else
			cat STDERR
		fi
	else
		echo "ERROR: Program not found!"
		let errors+=1
	fi
done

# check for successful session records
echo "... checking server records of successful sessions for $student"
for p in TCP TLS
do
	# retrieve the server log
	rm -f $SERVERLOG
	sfx="_SERVER"
	url=$BASE_URL/$p$sfx
	wget $url/$SERVERLOG 2> /dev/null
	if [ $? -ne 0 ]; then
		echo "ERROR: Unable to retrieve $SERVERLOG from $url"
		let errors+=1
		continue
	else
		echo "        retrieve $SERVERLOG from $url ... OK"
	fi

	# confirm session identification
	grep "SESSION STARTED: ID=$student" $SERVERLOG > /dev/null
	if [ $? -ne 0 ]; then
		echo "ERROR: No successful $p session establishements for $student"
		let errors+=1
		continue
	else
		echo "        confirm successful $p identification ... OK"
	fi
		
	# confirm completion
	grep "SESSION COMPLETED: ID=$student" $SERVERLOG > HITS
	if [ $? -ne 0 ]; then
		echo "ERROR: No successful $p session completions for $student"
		let errors+=1
		continue
	else
		echo "        confirm successful $p completion ... OK"
	fi

	# confirm a reasonable number of reports
	rpts=`tail -n1 HITS | cut -f3 -d=`
	tot=`echo $rpts | cut -f2 -d/`
	if [ $tot -lt $MIN_REPORTS ]; then
		echo "ERROR: only $tot $p reports received"
		let errors+=1
		continue
	fi

	good=`echo $rpts | cut -f1 -d/`
	if [ $good -ne $tot ]; then
	echo
		echo "ERROR: only $good/$tot valid $p reports"
		let errors+=1
	else
		echo "        good $p reports ... $good/$tot"
	fi
done


# that's all the tests I could think of
echo
if [ $errors -eq 0 ]; then
	echo "SUBMISSION $tarball ... passes sanity check"
	echo
	echo
	cleanup 0
else
	echo "SUBMISSION $tarball ... fails sanity check with $errors errors!"
	echo
	echo
	cleanup -1
fi
