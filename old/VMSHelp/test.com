$ mms/macro=(multinet=1, u=priam::)
$ def/nolog www$temp_dir disk$scratch:[pubd1.week.www]
$ helpgate :== $ disk$d1:[jfg.www]www_server
$ helpgate "-v" "-a" *:8000 -L sys$output