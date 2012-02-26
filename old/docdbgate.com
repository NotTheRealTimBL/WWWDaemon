$!	gateway allowing w3 access to docdb
$!
$!  This is, like many W3 gateways to older systems, a bit of a hack.
$!  Ifthe underlying database changes, then the person who does that
$!  can make a much neater W3 gateway when they do that.
$!  If noone does, then this can stand.
$!
$! Document identifier syntax:
$!
$!   /docdb/doc/PN-0001
$!   /docdb/anything?key+key    List documents under keywords
$ @docdb$root:[com]setup
$! set verify
$ socdev = p1
$ arg = p2
$ keys = p3
$ delim = "/"
$ cr[0,8]=13
$ lf[0,8]=10
$ nl = "''cr'''lf'"
$! write sys$output " writing to ''socdev'"
$! open/write client sys$output: 		! for inet daemon only
$ open/write client 'socdev' 		! for inet daemon only
$ write client "<isindex>"		! We always allow a query
$ if keys .nes. "" 
$ then
$!
$!			KEYWORD QUERY
$!
$     write client "<title>''keys' in DOCDB</title>''nl'"
$     title = keys 
$     write client "<h1>''keys'</h1>''nl'"
$!
$ next_key:
$     plus= f$locate("+", keys)
$     key = f$extract(0, plus, keys)
$     if plus .ne. f$length(keys)
$     then
$         keys = f$extract(plus+1, 132, keys)
$     else
$         keys = ""
$     endif
$     write client "<h2>''key'</h2>", nl
$     open/read/error=bad_key list docdb$doc_lists:list_of_'key'.txt
$     write client "<DL>''nl'"
$ loop:
$        read/error=endloop list line
$        if f$extract(1,1,line) .eqs. " "  ! skip col 1 altogether
$          then ! start with space
$            if f$extract(0,19,line).eqs."                   "
$            then
$              if f$extract(19,1,line).nes." "
$              then
$                write client f$extract(19,132,line), nl   ! Contnuation
$                goto loop  ! continue
$              endif
$            endif
$            ! (deal with title line? don't bother)
$          else ! start with non-space
$            if f$extract(8,1,line).eqs." "
$            then  !  Document description first line
$              doc = f$edit(f$extract(1,3,line), "collapse") + -
			"-" + f$extract(4,4,line) ! doc number
$              write client "<DT><A HREF=/docdb/doc/''doc'>"
$	       write client f$extract(1,8,line), "</A><DD>", nl
$              write client f$extract(19,132,line), nl
$            endif
$          endif ! start with non-space
$        goto loop
$     endloop:
$     write client "</DL>''nl'"
$   if keys .nes. "" then goto next_key   
$   exit
$!
$   bad_key:
$     write client """''key'"" is not a recognised keyword.", nl
$     write client "(See list of keywords -- to be specified)", nl
$     exit
$ endif
$!
$!			DOCUMENT RETRIEVAL
$!
$ scheme = f$element(1, delim, arg)
$ if scheme .nes. "docdb" then goto bad_id
$!
$ L1 = f$element(2, delim, arg)
$ if L1 .nes. delim
$ then
$    L2 = f$element(3, delim, arg)
$    if L1 .eqs. "doc"
$    then
$    
$     write client "<title>Document ''L2' in DOCDB</title>''nl'"
$     write client "<PLAINTEXT>''nl'"
$     open/read/error=sorry list docdb$documents:'L2'.doc
$ loop2:
$        read/error=endloop2 list line
$        write client line, nl
$        goto loop2
$ endloop2:
$        exit
$ sorry:
$        write client "Sorry, the document ''L2' is not available online''nl'"
$        write client "For a paper copy, please contact the computing", nl
$        write client "division library, (708) 840 3934", nl
$        exit
$    endif
$ endif ! No first level name
$! (falls though)
$ bad_id:
$    write client "<title>Fermilab DOCDB database</title>", nl
$    write client "<h1>DOCDB</h1>", nl
$    write client "You can search for documents by keyword.", nl
$    write client "More than one keyword will give you a separate list for", -
     " each keyword. Keywords include surnames of authors.", nl
$    write client "<p>Alternatively, you can read the (long) lists of", nl
$    write client "<a href=/docdb/?ALL>all documents</a>,", nl
$    write client "<a href=/docdb/?PN>supported software</a>,", nl
$    write client "<a href=/docdb/?HN>hardware notes</a>, and", nl
$    write client "<a href=/docdb/?IN>internal OLS notes</a>.", nl
$    write client "<p>", nl
$    write client "If you find any inconsistency in this data", nl
$    write client "please mail ruth@fnal.fnal.gov", nl
$    write client "", nl
$    write client "", nl
$    exit
