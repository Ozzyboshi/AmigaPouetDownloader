/* Script to download from pouet
*/

/*address ibrowse show*/

PARSE ARG pouetid pouetidend
IF pouetidend < pouetid THEN DO
   pouetidend=pouetid
END

/* MAKEDIR(), STATEF(), RENAME() and DELETE() live in rexxsupport.library */
IF ~SHOW('L','rexxsupport.library') THEN
  CALL ADDLIB('rexxsupport.library',0,-30,0)

if exists('pouet:') == 0 THEN DO
  SAY 'Please assign pouet:'
  EXIT
END

if exists('pouettmp:') == 0 THEN DO
  SAY 'Please assign pouettmp:'
  EXIT
END

/* Both set by the installer. pouetfetcher picks the download method:
   wget goes through the proxy PC, bebboget does HTTPS by itself and only
   falls back to wget and the proxy, if one is set, when it fails.
   Installations older than pouetfetcher have no such variable: wget. */
fetcher = GetEnv('pouetfetcher')
IF fetcher ~== 'bebboget' THEN fetcher = 'wget'

proxyaddress = GetEnv('pouetproxy')
IF LENGTH(proxyaddress) < 4 THEN proxyaddress = ''

IF fetcher == 'wget' & proxyaddress == '' THEN DO
  SAY 'NO PROXY ADDRESS FOUND, SET ONE,example setenv pouetproxy=x.x.x.x:9999'
  EXIT
END

IF fetcher == 'bebboget' & ~EXISTS('c:bebboget') THEN DO
  SAY 'pouetfetcher is bebboget but c:bebboget is missing, run the installer again'
  EXIT
END

IF fetcher == 'bebboget' THEN DO
  IF proxyaddress == '' THEN SAY 'Using bebboget, no proxy fallback'
  ELSE SAY 'Using bebboget, falling back to proxy address 'proxyaddress
END
ELSE SAY 'Using proxy address 'proxyaddress

IF pouetid = "party" THEN DO
  PARSE ARG pouetid pouetidend partyyearinput
  partyyearinput=STRIP(partyyearinput)
  partyurl = 'https://api.pouet.net/v1/party/?id='pouetidend'&year='partyyearinput
  IF Fetch(partyurl, 'pouettmp:pouetpartytmp.json', 0, 1) THEN MyReturnCode = 0
  ELSE MyReturnCode = 1
  if (MyReturnCode = 0) then
  do
    SAY 'Download Pouet party page seems OK'
    scanids = 'c:pouet_party pouettmp:pouetpartytmp.json --onlyid > pouettmp:pouetpartyids.txt'
   	address command scanids
    IF OPEN('ReqF', 'pouettmp:pouetpartyids.txt', 'R') THEN DO
    DO WHILE ~EOF('ReqF')
        linea = READLN('ReqF')
        linea = STRIP(linea)
        IF linea ~= '' THEN DO
            pouetcommand = 'rx pouet 'linea
            say pouetcommand
            address command pouetcommand
        END
    END
    CALL CLOSE 'ReqF'
END
ELSE DO

    SAY "Impossibile aprire il file"

END

  end
  EXIT
END


if arg() == 0 then do
	if exists('pouet:pouetlastid.txt') ==0 THEN DO
  		say 'cant find pouet:pouetlastid.txt'
		exit
	END

	open(ReqF,'pouet:pouetlastid.txt','r')
	pouetid = readln(ReqF)
	close(ReqF)

	say 'start downloading from 'pouetid
	
	IF Fetch('https://api.pouet.net/v1/front-page/latest-added/', 'pouettmp:pouettmp.json', 0, 1) THEN MyReturnCode = 0
	ELSE MyReturnCode = 1
	if (MyReturnCode = 0) then
  	do
  		reldate = 'pouet_lastid pouettmp:pouettmp.json > pouettmp:pouetlastprods.txt'
   		address command reldate
   		open(ReqF,'pouettmp:pouetlastprods.txt','r')
   		pouetidend = readln(ReqF)
   		say 'go up to ' pouetidend
   		close(ReqF)
   		if (pouetid > pouetidend) then
   		do
   			say 'already up to date'
   			exit
   		end
   	end
end



SAY 'Scraping pouet from id 'pouetid' to 'pouetidend

DO pouetid = pouetid TO pouetidend

if exists('pouettmp:pouettmp.html') THEN DO
  SAY 'Delete old pouet tmp files...'
  cmddel = 'delete pouettmp:pouet#? QUIET'
  address command cmddel
END

DESTDIR='pouet:'
/*SAY "enter pouet id"
PULL pouetid*/

IF Fetch('https://api.pouet.net/v1/prod/?id='pouetid, 'pouettmp:pouettmp.json', 0, 1) THEN MyReturnCode = 0
ELSE MyReturnCode = 1
if (MyReturnCode = 0) then
  do
   SAY 'Download Pouet page seems OK'

   /* For an id with no prod behind it, deleted or not there yet, the
      api answers with just { "error": true }. */
   IF OPEN('jsonf', 'pouettmp:pouettmp.json', 'R') THEN DO
     head = COMPRESS(READCH('jsonf', 64))
     CALL CLOSE 'jsonf'
     IF POS('"error":true', head) > 0 THEN DO
       SAY 'Prod 'pouetid' not found on pouet, skipping'
       ITERATE
     END
   END

   reldate = 'c:parse_prod pouettmp:pouettmp.json 5 > pouettmp:pouetreldate.txt'
   address command reldate
   open(ReqF,'pouettmp:pouetreldate.txt','r')
   reldatestr = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(reldatestr) == 0 THEN DO
   	SAY 'no production date'
	  ITERATE
   END
   SAY 'Release date is 'reldatestr
   
   titlestripped = 'c:parse_prod pouettmp:pouettmp.json 2 > pouettmp:pouetrtitle.txt'
   address command titlestripped
   open(ReqF,'pouettmp:pouetrtitle.txt','r')
   titlestripped = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(titlestripped) == 0 THEN DO
   	SAY 'no production title'
	ITERATE
   END
   SAY "Title is:" titlestripped

   partyname = 'c:parse_prod pouettmp:pouettmp.json 24 > pouettmp:pouetparty.txt'
   address command partyname
   open(ReqF,'pouettmp:pouetparty.txt','r')
   partyname = readln(ReqF)
   partyyearstripped = readln(ReqF)
   partyyearstripped = readln(ReqF)
   partyyearstripped = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(partyname) == 0 | partyname == '(no placing)' THEN DO
    partyname = 'no party'
   	SAY 'No party detected, putting in no party category'
    partydatecmd = 'c:parse_prod pouettmp:pouettmp.json 5 > pouettmp:pouetnoparty.txt'
    address command partydatecmd
    open(ReqF,'pouettmp:pouetnoparty.txt','r')
    partyyearstripped = readln(ReqF)
    partyyearstripped = substr(partyyearstripped,1,4)
    bs = close(ReqF)
   END
   IF LENGTH(partyyearstripped) == 0 THEN DO
    partyyearstripped = 'no year'
   	SAY 'No party year detected, putting in no year party category'
   END
   partystripped = partyname
   SAY "Party name is:" partystripped
   SAY "Year Party is:" partyyearstripped

   reltypestripped = 'c:parse_prod pouettmp:pouettmp.json 3 > pouettmp:pouetreltype.txt'
   address command reltypestripped
   open(ReqF,'pouettmp:pouetreltype.txt','r')
   reltypestripped = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(reltypestripped) == 0 THEN DO
   	SAY 'no production release type'
	  ITERATE
   END
   SAY "Rel type is:" reltypestripped

   relplatformstripped = 'c:parse_prod pouettmp:pouettmp.json 20 > pouettmp:pouetrelplatform.txt'
   address command relplatformstripped
   open(ReqF,'pouettmp:pouetrelplatform.txt','r')
   relplatformstripped = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(relplatformstripped) == 0 THEN DO
   	SAY 'no production release platform'
	  ITERATE
   END
   SAY "Rel platform is:" relplatformstripped

   pickstripped = 'c:parse_prod pouettmp:pouettmp.json 10 > pouettmp:pouetdownloadlink.txt'
   address command pickstripped
   open(ReqF,'pouettmp:pouetdownloadlink.txt','r')
   pickstripped = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(pickstripped) == 0 THEN DO
   	SAY 'no production download link'
	  ITERATE
   END
   SAY "DOWNLOAD URL IS:" pickstripped

   downloadable = 'yes'
   
   IF FIND(relplatformstripped,'Amiga') 
   THEN SAY 'This is an Amiga production.. downloading'
   ELSE DO
    SAY 'Not an Amiga production... skip'
    downloadable = 'no'
   END   
   
   if downloadable == 'yes' then do
   

   if 1 then

      sceneorg = substr(pickstripped,1,29)
      if sceneorg == 'https://files.scene.org/view/' THEN DO
        sceneorg2 = substr(pickstripped,29)
        SAY 'Scene is 'sceneorg
        SAY 'Scene 2 is 'sceneorg2
        pickstripped  = 'https://files.scene.org/get/'sceneorg2
        SAY 'pickstripped is 'pickstripped
      END

      extension = substr(pickstripped,length(pickstripped)-3)
      SAY "Detected extension is ###"extension"###"

      targetdir = DESTDIR || 'parties/' || partystripped || '/' || ,
            partyyearstripped || '/' || reltypestripped || '/' || ,
            titlestripped
       
      if exists(targetdir) then DO
        say "target dir already there, skipping creation"
      END
      ELSE DO
        SAY 'Creating directory:' targetdir
        CALL MakeDirAll targetdir
      END

      defaultaction=1

      if extension == ".adf" THEN DO
        SAY "alessio Detected extension is ###"extension"###"
        defaultaction=0
        SAY "Downloading an ADF file"
        CALL Fetch pickstripped, 'pouettmp:adfmount.adf', 0, 0

        say "check if exists"

        adfdir = targetdir || '/adf'
        if exists(adfdir) then do
          say "Download dir already there, reusing..."
        end
        else do
          say 'Creating directory:' adfdir
          CALL MakeDirAll adfdir
        end

        download = 'c:adf_extract pouettmp:adfmount.adf "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/adf"' 
        say download
        address command download

        download = 'copy pouettmp:adfmount.adf to "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/'titlestripped'.adf"' 
        say download
        address command download
      END

      if extension == ".lha" THEN DO
        SAY 'This is an lha compressed file'
        CALL Fetch pickstripped, 'pouettmp:pouetdownload.lha', 0, 0
        unlha = 'lha x pouettmp:pouetdownload.lha "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/"'
        address command unlha
        say unlha
        bs = close(ReqF)
        defaultaction=0
      END

      if extension == '.dms' THEN DO
        SAY 'This is a dms compressed file'
        CALL Fetch pickstripped, 'pouettmp:pouetdownload.dms', 0, 0
        undms = 'xdms -d "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/" u pouettmp:pouetdownload.dms'
        address command undms
        say undms
        bs = close(ReqF)
        defaultaction=0
      END

      if extension == '.DMS' THEN DO
        SAY 'This is a DMS compressed file'
        CALL Fetch pickstripped, 'pouettmp:pouetdownload.dms', 0, 0
        undms = 'xdms -d "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/" u pouettmp:pouetdownload.dms'
        address command undms
        say undms
        bs = close(ReqF)
        defaultaction=0
      END

      if extension == ".zip" THEN DO
        SAY "This is a zipped file, unzipping..."
        CALL Fetch pickstripped, 'pouettmp:pouetdownload.zip', 0, 0
        /*if RC = 0 THEN
          SAY 'Download succeded'
        ELSE
          SAY 'Download failed'
        END
        */


        /*unzip = 'unzip -n pouettmp:pouetdownload.zip -d ''"pouettmp:parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped"'*/
        unzip = 'unzip -n pouettmp:pouetdownload.zip -d "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'"'
        say unzip
        address command unzip
         
        bs = close(ReqF)
        defaultaction=0

      END

      IF defaultaction==1 THEN DO
        SAY "Downloading default file, probably executable"
        CALL Fetch pickstripped, targetdir, 1, 0

	      setexecutable = 'protect "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/#?" ' '+e'
	      SAY setexecutable
	      address command setexecutable
        bs = close(ReqF)
      END
       
    END
    
    if exists('pouet:pouetlastid.txt') then do
      open(ReqF,'pouet:pouetlastid.txt','r')
      pouetidold = readln(ReqF)
      close(ReqF)
    end
    else do
      pouetidold = 0
    end
    if pouetidold < pouetid +1 THEN DO
    	open(ReqF,'pouet:pouetlastid.txt','w')
    	say 'updating lastpouetid with 'pouetid+1
    	writeln( ReqF, pouetid+1)
    	close(ReqF)
    END

  end
  else
  do
    SAY 'Download Failed'
  end
SAY 'response >' MyReturnCode
END /* END LOOP */
EXIT


/* Value of environment variable name, read straight from ENV:, or '' if
   it is not set. */
GetEnv: PROCEDURE
  PARSE ARG name
  value = ''
  IF OPEN('envf', 'ENV:'name, 'R') THEN DO
    value = STRIP(READLN('envf'))
    CALL CLOSE 'envf'
  END
  RETURN value


/* Download url with the configured method. target is a file name, or with
   todir=1 the directory to save into under the name the url ends with.
   quiet=1 hides the progress output. Returns 1 on success, 0 on failure. */
Fetch: PROCEDURE EXPOSE fetcher proxyaddress
  PARSE ARG url, target, todir, quiet
  IF fetcher == 'bebboget' THEN DO
    file = target
    IF todir THEN file = target'/'UrlFileName(url)
    IF BebboFetch(url, file, quiet) THEN RETURN 1
    IF proxyaddress == '' THEN RETURN 0
    SAY 'Direct download failed, retrying through the proxy' proxyaddress
  END
  RETURN WgetFetch(url, target, todir, quiet)


/* wget through the proxy PC, which does HTTPS and redirects for us. */
WgetFetch: PROCEDURE EXPOSE proxyaddress
  PARSE ARG url, target, todir, quiet
  cmd = 'c:wget'
  IF quiet THEN cmd = cmd '--quiet'
  IF todir THEN cmd = cmd '-P "'target'"'
  ELSE cmd = cmd '-O "'target'"'
  cmd = cmd '"'proxyaddress'/'url'"'
  IF ~quiet THEN SAY cmd
  ADDRESS COMMAND cmd
  RETURN RC = 0


/* bebboget straight to the server. --sloppy skips certificate checks, as
   the proxy does. bebboget exits with 0 even when the download fails, so
   the outcome is read from the response headers that -S prints: the last
   status line wins, since redirects print one per hop. It also leaves
   chunked transfer encoding in the file, which is undone here. */
BebboFetch: PROCEDURE
  PARSE ARG url, file, quiet
  url = Replace(url, '&amp;', '&')
  hdr = 'pouettmp:pouethdr.txt'
  cmd = 'c:bebboget --sloppy -q -O -S -o "'file'" "'url'"'
  IF ~quiet THEN SAY cmd
  ADDRESS COMMAND cmd '>'hdr

  status = 0
  chunked = 0
  IF OPEN('hdrf', hdr, 'R') THEN DO
    DO WHILE ~EOF('hdrf')
      line = READLN('hdrf')
      IF LEFT(line, 7) == '< HTTP/' THEN DO
        PARSE VAR line . . status .
        chunked = 0
      END
      ELSE IF POS('TRANSFER-ENCODING: CHUNKED', UPPER(line)) > 0 THEN chunked = 1
      ELSE IF POS('[ERROR]', line) > 0 THEN SAY line
    END
    CALL CLOSE 'hdrf'
  END

  IF status ~= 200 THEN DO
    SAY 'bebboget failed, HTTP status' status
    RETURN 0
  END
  IF chunked THEN
    IF ~Dechunk(file) THEN DO
      SAY 'Cannot decode chunked download' file
      RETURN 0
    END
  info = STATEF(file)
  RETURN info ~== '' & WORD(info, 2) > 0


/* Turn a file saved with chunked transfer encoding back into the plain
   body: each chunk is a hex size line, the data and a CRLF, and a zero
   size ends it. Returns 1 on success, 0 on failure. */
Dechunk: PROCEDURE
  PARSE ARG file
  tmp = file'.dechunk'
  IF ~OPEN('chin', file, 'R') THEN RETURN 0
  IF ~OPEN('chout', tmp, 'W') THEN DO
    CALL CLOSE 'chin'
    RETURN 0
  END

  ok = 1
  DO FOREVER
    IF EOF('chin') THEN DO
      ok = 0
      LEAVE
    END
    line = STRIP(STRIP(READLN('chin'), 'T', '0D'x))
    IF line == '' THEN ITERATE
    PARSE VAR line hex ';' .
    IF ~DATATYPE(hex, 'X') THEN DO
      ok = 0
      LEAVE
    END
    remain = X2D(hex)
    IF remain = 0 THEN LEAVE
    DO WHILE remain > 0
      data = READCH('chin', MIN(remain, 32768))
      IF data == '' THEN LEAVE
      CALL WRITECH 'chout', data
      remain = remain - LENGTH(data)
    END
    IF remain > 0 THEN DO
      ok = 0
      LEAVE
    END
  END

  CALL CLOSE 'chin'
  CALL CLOSE 'chout'
  IF ok THEN DO
    CALL DELETE file
    ok = RENAME(tmp, file)
  END
  ELSE CALL DELETE tmp
  RETURN ok


/* Last path component of url, without the query string. */
UrlFileName: PROCEDURE
  PARSE ARG url
  PARSE VAR url path '?' .
  name = SUBSTR(path, LASTPOS('/', path) + 1)
  IF name == '' THEN name = 'download'
  RETURN name


/* str with every occurrence of old replaced by new. */
Replace: PROCEDURE
  PARSE ARG str, old, new
  out = ''
  DO WHILE POS(old, str) > 0
    p = POS(old, str)
    out = out || LEFT(str, p - 1) || new
    str = SUBSTR(str, p + LENGTH(old))
  END
  RETURN out || str


/* Create directory path, including any missing parents (like mkdir -p).
   MAKEDIR() only creates one level, so walk the path one component at a
   time after the volume/assign part. Returns 1 on success, 0 on failure. */
MakeDirAll: PROCEDURE
  PARSE ARG path
  path = STRIP(path, 'T', '/')
  IF EXISTS(path) THEN RETURN 1

  colon = POS(':', path)
  done = LEFT(path, colon)
  rest = SUBSTR(path, colon + 1)

  DO WHILE rest ~= ''
    PARSE VAR rest part '/' rest
    IF part == '' THEN ITERATE
    IF done == '' | RIGHT(done, 1) == ':' THEN done = done || part
    ELSE done = done || '/' || part
    IF ~EXISTS(done) THEN DO
      IF ~MAKEDIR(done) THEN DO
        SAY 'Cannot create directory' done
        RETURN 0
      END
    END
  END
  RETURN 1
