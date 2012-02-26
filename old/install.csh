#! /bin/csh
#
#			Install HTTP daemon
#
#  This script works for regular BSD unix systems.  It will not
#  necessarily work with fancy bells and whistles.
#  Check all the places where BSD is mentioned for differences for sys V
#  If your inted doesn't support running a daemon under otherthan root,
#	change the format of the inted.conf entry near the bottom
#  Tested on: NeXT_3.0
#
#  Supposed to work with NeXT netinfo.
#  Supposed to work with yp
#
#  Read first then run it as root
#  Assumes binaries are in WWW_MACH directories parallel with Implementation/
#  Running it more than once won't harm anything.
#  You can run it if the server is already partially installed,
#  and it will try to fix up what is missing.

set prod = httpd
set service = www
set port = 80

set yes = 1
set no = 0
set signal_inetd = $no

#		Copy over daemon


if ( "$WWW_MACH" == "" ) then
    echo You must first setenv WWW_MACH to give your machine type
    exit
endif

if ( -e ../$WWW_MACH/$prod ) then
    cp ../$WWW_MACH/$prod /usr/etc/$prod
else
    echo There is no ../$WWW_MACH/$prod daemon executable file for $WWW_MACH
    echo Try making it first.
    exit 2
endif

#		Make configuration file

if ("$prod" == "httpd") then
 
    if ( -e /etc/httpd.conf ) then
	echo The configuration file /etc/httpd.conf already exists.
	set ans =
	while (("$ans" != "y") && ("$ans" != "n"))
	    glob "Overwrite with default version? (y or n) "
	    set ans = $<
	end
	if ($ans == "n") goto conf_done
	if ( ! -e /etc/httpd.conf.bak ) then
	    mv /etc/httpd.conf /etc/httpd.conf.bak
	else
	    rm -f /etc/httpd.conf
	endif
    
    else
	cat <<EOF >> /etc/httpd.conf
    #	Configuration file for World Wide Web server
    map   /     /welcome.html
    map   /*     /Public/*
    pass  /Public/*
    fail  *
    EOF
    endif
    
    conf_done:

#		Make home page

    if ( ! -r /Public ) mkdir /Public
    if (   -r /Public/welcome.html ) then
	echo /Public/welcome.html already exists .. not overwritten.
    else
	echo "<title>Welcome to" `hostname`" W3 server</title>" \
		>/Public/welcome.html
	echo "<h1>Welcome to" `hostname` "</h1>" >>/Public/welcome.html
	cat <<EOF >>/Public/welcome.html
    This server has just been installed, and no local
    data is currently available. Edit me!
    Watch this space...
    <h2>Information elsewhere</h2>
    For other information,
    <a href=http://info.cern.ch/>select this link</a>.
    <p>
    <address>Server Manager<address>
    EOF
    endif

endif

#   Put $service into netinfo or /etc/service if necessary

#   netinfo services:
if ( -d /etc/netinfo ) then
    echo You have netinfo, so we will use that.
    niutil -read . services/$service >/dev/null
    if ($status == 0) then
        echo Service $service defined already
    else
        niutil -create . services/$service
	signal_inetd = $yes
    endif
    niutil -createprop . services/$service port 80
    niutil -createprop . services/$service protocol tcp
else
#    real  /etc/services file
    grep "^[^ \t]*[ \t]*$port/tcp" /etc/services >/dev/null
    if ($status == 0) then
	set service = ( `grep "^[^ \t]*[ \t]*$port/tcp" /etc/services` )
	set service = $service[1]
	echo "  Port 80 service is already defined as" $service
    else
	echo "Adding service $service on port $port"
	echo "$service	$port/tcp	# World-Wide Web information service" /
		 >> /etc/services
	if ( -r /var/yp ) then
	    echo You seem to have yellow pages. Rebuilding yp for services
	    ( cd /var/yp; make )
	    echo If this doesn't work, try a reboot :-)
	endif
    endif
endif

#   		Put httpd into inetd.conf if not in already

grep "^${service}" /etc/inetd.conf > /dev/null
if ($status == 0) then
    set server = ( `grep "^${service}" /etc/inetd.conf` )
#   check the index 6 below if not bsd
    echo $service daemon already defined to be $server[6]
else
    cat <<EOF >> /etc/inetd.conf
#  WorldWide Web service
$service	stream	tcp	nowait	nobody	/usr/etc/$prod prod
EOF
    echo The daemon has been added to the inetd configuration file.
    signal_inetd = $yes
endif

#   		Send singal to inetd to make it reread inetd.conf


if ( $signal_inetd ) then
    echo 
#   @@@ BSD only: check which column the process number is in from ps aux!
#   @@@ BSD and the aux should be -es or someting in SysV
    set inetd = ( `ps aux | grep inetd | grep -v grep` )
    echo Sending HUP signal to inetd process $inetd[2] 
    kill -HUP $inetd[2]
endif

#		Test it ... requires www to be installed.

if ( -x ../../LineMode/$WWW_MACH/www ) then
    echo Testing new server...
    set here = `hostname`
    ../../LineMode/$WWW_MACH/www -n http://$here/
    if ($status != 0) then
	echo "****" TEST FAILED
	exit 3
    else
	echo WWW3 server on `hostname` Tested aparently OK.
    endif
else
    echo Can't test it: no www in ../../LineMode/$WWW_MACH
endif

#	    Log it to www team at cern to keep some statistics.

mail -s "$here installed W3 server " \
	www-log@info.cern.ch < $0
exit
