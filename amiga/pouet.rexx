/* Script to download from pouet
*/

/*address ibrowse show*/

PARSE ARG pouetid pouetidend
IF pouetidend < pouetid THEN DO
   pouetidend=pouetid
END

if exists('pouet:') == 0 THEN DO
  SAY 'Please assign pouet:'
  EXIT
END

address command 'getenv pouetproxy > ram:pouetproxy.txt'
open(ReqF,'ram:pouetproxy.txt','r')
proxyaddress = readln(ReqF)
close(ReqF)

if length(proxyaddress) < 4 THEN DO
  SAY 'NO PROXY ADDRESS FOUND, SET ONE,example setenv pouetproxy=x.x.x.x:9999'
  EXIT
END

if proxyaddress == 'object not found' THEN DO
  SAY 'NO PROXY ADDRESS FOUND, SET ONE,example setenv pouetproxy=x.x.x.x:9999'
  EXIT
END

SAY 'Using proxy address 'proxyaddress


if arg() == 0 then do
	if exists('pouet:pouetlastid.txt') ==0 THEN DO
  		say 'cant find pouet:pouetlastid.txt'
		exit
	END

	open(ReqF,'pouet:pouetlastid.txt','r')
	pouetid = readln(ReqF)
	close(ReqF)

	say 'start downloading from 'pouetid
	
	cline = 'c:wget --quiet -O ram:pouettmp.html 'proxyaddress'/https://www.pouet.net/prodlist.php?order=added'
	address command cline
	MyReturnCode = RC
	if (MyReturnCode = 0) then
  	do
  		reldate = 'grep -o -E "prod.php\??which=[0-9]+" ram:pouettmp.html > ram:pouetlastid.txt'
   		address command reldate
   		open(ReqF,'ram:pouetlastid.txt','r')
   		reldatestr = readln(ReqF)
   		pouetidend = substr(reldatestr,16,length(reldatestr)-15)
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

if exists('ram:pouettmp.html') THEN DO
  SAY 'Delete old pouet tmp files...'
  cmddel = 'delete ram:pouet#? QUIET'
  address command cmddel
END

DESTDIR='pouet:'
/*SAY "enter pouet id"
PULL pouetid*/

cline = 'c:wget --quiet -O ram:pouettmp.json 'proxyaddress'/https://api.pouet.net/v1/prod/?id='pouetid

address command cline
MyReturnCode = RC
if (MyReturnCode = 0) then
  do
   SAY 'Download Pouet page seems OK'
   
   reldate = 'c:parse_prod ram:pouettmp.json 5 > ram:pouetreldate.txt'
   address command reldate
   open(ReqF,'ram:pouetreldate.txt','r')
   reldatestr = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(reldatestr) == 0 THEN DO
   	SAY 'no production date'
	  ITERATE
   END
   SAY 'Release date is 'reldatestr
   
   titlestripped = 'c:parse_prod ram:pouettmp.json 2 > ram:pouetrtitle.txt'
   address command titlestripped
   open(ReqF,'ram:pouetrtitle.txt','r')
   titlestripped = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(titlestripped) == 0 THEN DO
   	SAY 'no production title'
	ITERATE
   END
   SAY "Title is:" titlestripped

   partyname = 'c:parse_prod ram:pouettmp.json 24 > ram:pouetparty.txt'
   address command partyname
   open(ReqF,'ram:pouetparty.txt','r')
   partyname = readln(ReqF)
   partyyearstripped = readln(ReqF)
   partyyearstripped = readln(ReqF)
   partyyearstripped = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(partyname) == 0 THEN DO
    partyname = 'no party'
   	SAY 'No party detected, putting in no party category'
   END
   IF LENGTH(partyyearstripped) == 0 THEN DO
    partyyearstripped = 'no year'
   	SAY 'No party year detected, putting in no year party category'
   END
   partystripped = partyname
   SAY "Party name is:" partystripped
   SAY "Year Party is:" partyyearstripped

   reltypestripped = 'c:parse_prod ram:pouettmp.json 3 > ram:pouetreltype.txt'
   address command reltypestripped
   open(ReqF,'ram:pouetreltype.txt','r')
   reltypestripped = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(reltypestripped) == 0 THEN DO
   	SAY 'no production release type'
	  ITERATE
   END
   SAY "Rel type is:" reltypestripped

   relplatformstripped = 'c:parse_prod ram:pouettmp.json 20 > ram:pouetrelplatform.txt'
   address command relplatformstripped
   open(ReqF,'ram:pouetrelplatform.txt','r')
   relplatformstripped = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(relplatformstripped) == 0 THEN DO
   	SAY 'no production release platform'
	  ITERATE
   END
   SAY "Rel platform is:" relplatformstripped

   pickstripped = 'c:parse_prod ram:pouettmp.json 10 > ram:pouetdownloadlink.txt'
   address command pickstripped
   open(ReqF,'ram:pouetdownloadlink.txt','r')
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
     if 1 then do

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
       
       makedirectory = 'MKDIR "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'"'
       SAY 'Creating directory:' makedirectory
       address command makedirectory

       defaultaction=1
       if extension == ".adf" THEN DO
         gotourl save pickstripped
         SAY "Downloading an ADF file"
         
         download = 'wget --quiet --user-agent="Mozilla/5.0" -t 1 -O "ram:adfmount.adf" 'proxyaddress'/'pickstripped
         address command download
         
         download = 'mkdir pouet:adfextract'pouetid
         address command download
         
         download = 'unadf ram:adfmount.adf -d pouet:adfextract'pouetid
         address command download
         unadfout = RC
         say unadfout
         
         download = 'mkdir -p "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/adf" ALL' 
         say download
         address command download
         
         download = 'copy pouet:adfextract'pouetid' "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/adf" ALL' 
         say download
         address command download
         
         download = 'wget --quiet --user-agent="Mozilla/5.0" -t 1 -P "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'" 'proxyaddress'/'pickstripped
         address command download
         bs = close(ReqF)
         defaultaction=0
         #unadf = 'unadf  "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/'pickstripped'" -d ram:'
         #address command unadf
         #say unadf
         
         download = 'delete pouet:adfextract'pouetid' all'
         address command download
         
         download = 'delete pouet:adfextract'pouetid'.info all'
         address command download
         
         download = 'delete pouet:adfextract'pouetid' all'
         address command download


         

       END

       if extension == ".lha" THEN DO
         SAY 'This is an lha compressed file'
         download = 'wget -q -O ram:pouetdownload.lha "'proxyaddress'/'pickstripped'"'
         SAY "Download is ###"download"###"
         address command download
         unlha = 'lha x ram:pouetdownload.lha "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/"'
         address command unlha
         say unlha
         bs = close(ReqF)
         defaultaction=0

       END

       if extension == '.dms' THEN DO
         SAY 'This is a dms compressed file'
         download = 'wget --user-agent="Mozilla/5.0" -q -O ram:pouetdownload.dms 'proxyaddress'/'pickstripped
         address command download
         undms = 'xdms -d "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/" u ram:pouetdownload.dms'
         address command undms
         say undms
         bs = close(ReqF)
         defaultaction=0
       END

       if extension == '.DMS' THEN DO
         SAY 'This is a dms compressed file'
         download = 'wget --user-agent="Mozilla/5.0" -q -O ram:pouetdownload.dms 'proxyaddress'/'pickstripped
         address command download
         undms = 'xdms -d "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/" u ram:pouetdownload.dms'
         address command undms
         say undms
         bs = close(ReqF)
         defaultaction=0
       END



       if extension == ".zip" THEN DO

         SAY "This is a zipped file, unzipping..."
         download = 'c:wget --quiet -O ram:pouetdownload.zip 'proxyaddress'/'pickstripped
         SAY download
         address command download
         /*if RC = 0 THEN
           SAY 'Download succeded'
         ELSE
           SAY 'Download failed'
         END
         */


         /*unzip = 'unzip -n ram:pouetdownload.zip -d ''"ram:parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped"'*/
         unzip = 'unzip -n ram:pouetdownload.zip -d "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'"'
         say unzip
         address command unzip
         
         bs = close(ReqF)
         defaultaction=0

       END

       IF defaultaction==1 THEN DO
         SAY "Downloading default file, probably executable"
         download = 'wget --quiet -P "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'" 'proxyaddress'/'pickstripped
         SAY download
         address command download

	 setexecutable = 'protect "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'/#?" ' '+e'
	 SAY setexecutable
	 address command setexecutable
         bs = close(ReqF)
       END
       
    END
    END
    
    open(ReqF,'pouet:pouetlastid.txt','r')
    pouetidold = readln(ReqF)
    close(ReqF)
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
