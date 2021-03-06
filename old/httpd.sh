#!/bin/sh5
#
# Experimental http server for STING, started by inetd. DMS Mar 17 92
# Should have s bit set to avoid inetd executing it with owner root.
# The carriage returns are supposed to be for telnet compatibility.
# Doesn't make any attempt to shorten long lines.
#
CR=''
#
echo "<P>Testing STING server - results are unpredictable! $CR"; echo "<P>$CR"
#
# Get request line and check command
#
read command docid
#
if [ "$command" != "GET" ]
   then echo "<P>Unrecognised request received: $CR"
	echo "<P>$command $docid $CR"
	exit
fi
#
# Get rid of trailing CR if any
#
docid=`echo "$docid" | tr -d "$CR"`
#
# Look for question mark
#
query=`echo "$docid" | fgrep '?'`
if [ -z "$query" ]
#
#  Normal file request - check it exists and is readable
#
   then if [ ! -f "$docid" ]
	   then echo "<P>Document $docid not found $CR"
		exit
	fi
#
#  (test -r doesn't work properly in these circumstances,
#   so use condition code returned by head instead)
#
   head -1 "$docid" > /dev/null 2> /dev/null
   if [ "$?" != 0 ]
      then echo "<P>Access to $docid not authorised $CR"
	   exit
   fi
#
#  OK - check type and send it if .html or .txt
#
   type=`echo "$docid" | sed 's/^.*\.\(.*\)$/\1/'`
   if [ "$type" = 'txt' ]
      then echo "<PLAINTEXT>$CR"
   fi
   if [ "$type" = 'html' -o "$type" = 'txt' ]
      then sed "s/$/$CR/" "$docid"
      else echo "<P>$docid not hypertext or plain text file $CR"
   fi
   exit
fi
#
#  Deal with search if it's a query
#
#  Get the filename part of the docid string
#  and extract the name of the file to be searched,
#  using convention that filename "thing-anything.suffix"
#  implies a search in "thing.suffix"
#
filename=`echo "$docid" | cut -d? -f1`
target=`basename $filename | sed 's/-.*\././'`
searchname=`dirname $filename`/$target
# echo "<P>Searching in $searchname $CR"
#
# Get keywords and tidy up - the stuff with + etc is a bit messy.
# We want to protect ++ and trailing + which can't be generated by
# the browser, so must be part of the user's keyword. We can be sure
# at this point that there are no spaces or CRs in the string,
# so we can use these as temporary substitutions in the sed pattern.
#
keywords=`echo "$docid" | cut -d? -f2- | sed "
s/++/ /g
s/+$/$CR/
s/+/-/g
s/$CR/+/
s/ /++/g"`
if [ -z "$keywords" ] # null keyword
   then echo "<P>No entry found $CR"
	exit
fi
#
# Deal with search types
#
case "$target" in
     glossary*)
#
#    glossary-style search
#
	  found=`fgrep -i "NAME=$keywords>" "$searchname"`
	  if [ -z "$found" ] # relax criteria and try again
	     then keywords=`echo "$keywords" | sed 's/s$//'`
		  found=`fgrep -i "NAME=$keywords" "$searchname"`
		  if [ -z "$found" ]
		     then echo "<P>No entry found for $keywords $CR"
		     exit
		  fi
	  fi
#
#         we made it! send off material found in search
#
	  echo "<ISINDEX>$CR"
	  echo "<TITLE>Entry $keywords in $searchname </TITLE>$CR"
	  echo "<P>$CR"
	  echo "<DL>$CR"
	  echo "$found" | sed "s/$/$CR/"
	  echo "</DL>$CR"
	  echo "<P>$CR"
     ;;
     *)
	  echo "<P>Sorry, cannot do this type of search $CR"
     ;;
esac
exit
