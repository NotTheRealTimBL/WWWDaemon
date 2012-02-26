$ def www$temp_dir disk$scratch:[pubd1.week.www]
$ helpgate :== $ disk$d1:[jfg.www]www_server
$ helpgate "-a" *:80 -L sys$output
