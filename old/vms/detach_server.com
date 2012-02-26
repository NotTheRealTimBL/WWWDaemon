$! This procedure creates a detached process called WWW_SERVER
$! which provides hypertext access to VMS files
$!
$ run sys$system:loginout.exe -
      /detached -
      /uic=[jfg] -
      /process_name="WWW_Server" -
      /input=disk$d1:[jfg.www]www_server.com -
      /output=disk$d1:[jfg.www]www_server.log
$ exit
