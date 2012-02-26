#! /bin/c=sh
#
#	Install HTTP daemon
#
#  This script works for regular unix systems.  It will not
#  necessarily work with fancy bells and whistles.
#


cp httpd /usr/etc/httpd
if [ -e /etc/httpd.conf ]
then
    echo /etc/httpd.conf already exists. I assume it is OK.
cat <<EOF >> /etc/httpd.conf
#	Configuration file for World Wide Web server
map   /     /welcome.html
map   /*     /Public/*
pass  /Public/*
fail  *
EOF

if [ ! -r /Public ] mkdir /Public
if [ ! -r /Public/welcome.html]
then
    echo /Public/welcome.html already exists .. not overwritten.
else
    echo "<title>Welcome to" `hostname`" W3 server</title>" \
	 >/Public/welcome.html
    echo "<h1>Welcome to" `hostname`</h1> >>/Public/welcome.html
    cat <<EOF >>/Public/welcome.html
This server has just been installed, and no local
data is currently available.
Watch this space...
<h2>Information elsewhere</h2>
For other information,
<a href=http://info.cern.ch/>select this link</a>.
<p>
<address>Server Manager<address>
EOF
fi
#   Put www into netinfo or /etc/service if necessary
#
#   netinfo services:
if ( -d /etc/netinfo )
then
    echo You have netinfo, so we will use that.
    if niutil -read . services/www
    then
        echo Service 80 defined already
    else
        niutil -create . services/www
    fi
    niutil -createprop . services/www port=80 protocol=tcp
else
#    real  /etc/services file
    if grep "^[^ \t]*[ \t]*80/tcp" /etc/services
    then
	service=`grep "^[^ \t]*[ \t]*80/tcp" /etc/services`
	echo "  Port 80 service already defined as " $service
    else
	service=www
	echo "Adding service www on port 80"
	cat <<EOF >> /etc/services
#   World Wide web server added by installation script.
www	80/tcp		# World-Wide Web information service
EOF

    fi
fi
exit

#   Now put httpd into inetd.conf if not in already
if grep "^$service[ \t]" /etc/inetd.conf
then
    server=`grep "^$service[ \t]" /etc/inetd.conf
....

#    Log it to www team at cern to keep some statistics.
mail -s "W3 server instalation script run" www-bug@info.cern.ch < /dev/null
exit
