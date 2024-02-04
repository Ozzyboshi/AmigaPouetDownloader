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

cline = 'c:wget --quiet -O ram:pouettmp.html 'proxyaddress'/https://www.pouet.net/prod.php?which='pouetid

address command cline
MyReturnCode = RC
if (MyReturnCode = 0) then
  do
   SAY 'Download Pouet page seems OK'
   
   reldate = 'grep -A1 -E "<td>release date :</td>" ram:pouettmp.html > ram:pouetreldate.txt'
   address command reldate
   open(ReqF,'ram:pouetreldate.txt','r')
   reldatestr = readln(ReqF)
   reldatestr = readln(ReqF)
   bs = close(ReqF)
   IF LENGTH(reldatestr) == 0 THEN DO
   	SAY 'no production'
	ITERATE
   END
  
   reldatestr = substr(reldatestr,7,length(reldatestr)-11)
   SAY 'Release date is 'reldatestr
   
   title = 'grep -o -E "<span id=.prod-title.>(.*)</span>" ram:pouettmp.html > ram:pouettitle.txt'
   address command title
   
   title = 'grep -o -E ">.+</span> by" ram:pouettitle.txt > ram:pouettitle2.txt'
   address command title
   
   open(ReqF,'ram:pouettitle2.txt','r')
   title = readln(ReqF)
   IF LENGTH(title) == 0 THEN DO
     bs = close(ReqF)
     open(ReqF,'ram:pouettitle.txt','r')
     title = readln(ReqF)
     titlestripped = substr(title,23,length(title)-36)
   END
   ELSE
    titlestripped = substr(title,2,length(title)-11)
   SAY "Title is:" titlestripped
   bs = close(ReqF)

   party = 'grep -E "party.php" ram:pouettmp.html > ram:pouetparty.txt'
   address command party

   partyname = 'grep -E -o ">(.*)</a>" ram:pouetparty.txt > ram:pouetname.txt'
   address command partyname
   
   partyname = 'grep -E -o "<(.*)</a>" ram:pouetname.txt > ram:pouetname2.txt'
   address command partyname
   
   partyname = 'grep -E -o ">(.*)</a>" ram:pouetname2.txt > ram:pouetname3.txt'
   address command partyname
   
   partystripped = 'no party'
   open(ReqF,'ram:pouetname3.txt','r')
   partyname = readln(ReqF)
   if LENGTH(partyname) == 0 
     THEN SAY 'No party detected, putting in no party category'
   ELSE DO
     partystripped = substr(partyname,2,length(partyname)-5)
     SAY "Party is:" partystripped
     partyname = 'grep -E -o "</a>(.*)" ram:pouetparty.txt > ram:pouetyear.txt'
     address command partyname
   END
   bs = close(ReqF)
   
   partyyearstripped='no year'
   if LENGTH(partyname) == 0 THEN DO
     SAY 'No party year detected, using release date 'reldatestr
     partyyearstripped = reldatestr
   END
   ELSE DO
     open(ReqF,'ram:pouetyear.txt','r')
     partyyear = readln(ReqF)
     partyyearstripped = substr(partyyear,6,length(partyyear)-10)
     SAY "Year Party is:" partyyearstripped
   END
   bs = close(ReqF)
   
   reltype = 'grep -o -E "prodlist.php.type(.*)" ram:pouettmp.html > ram:pouettype.txt'
   address command reltype
   
   reltype = 'grep -o -E "><(.*)" ram:pouettype.txt > ram:pouettype2.txt'
   address command reltype
   
   reltype = 'grep -o -E "<(.*)" ram:pouettype2.txt > ram:pouettype3.txt'
   address command reltype
   
   reltype = 'grep -o -E ">(.*)</span>" ram:pouettype3.txt > ram:pouettype4.txt'
   address command reltype
   
   open(ReqF,'ram:pouettype4.txt','r')
   reltype = readln(ReqF)
   reltypestripped = substr(reltype,2,length(reltype)-8)
   SAY "Rel type is:" reltypestripped
   bs = close(ReqF)
   
   
   
   
   relplatform = 'grep -E -o "prodlist.php.platform(.*)" ram:pouettmp.html > ram:pouetplatform.txt'
   address command relplatform
   
   relplatform = 'grep -o -E "><(.*)" ram:pouetplatform.txt > ram:pouetplatform2.txt'
   address command relplatform
   
   relplatform = 'grep -o -E "<(.*)" ram:pouetplatform2.txt > ram:pouetplatform3.txt'
   address command relplatform
   
   relplatform = 'grep -o -E ">(.*)</span>" ram:pouetplatform3.txt > ram:pouetplatform4.txt'
   address command relplatform
   
   open(ReqF,'ram:pouetplatform4.txt','r')
   relplatform = readln(ReqF)
   relplatformstripped = substr(relplatform,2,length(relplatform)-8)
   SAY "Rel platform is:" relplatformstripped
   bs = close(ReqF)

   downloadable = 'yes'
   
   IF FIND(relplatformstripped,'Amiga') 
   THEN SAY 'This is an Amiga production.. downloading'
   ELSE DO
    SAY 'Not an Amiga production... skip'
    downloadable = 'no'
   END   


   
   downloadrow = 'grep -E mainDownloadLink ram:pouettmp.html > ram:pouetline.txt'
   address command downloadrow

   strip1 = 'grep -o -e href=.*> ram:pouetline.txt > ram:pouetline2.txt '
   address command strip1
   
   if downloadable == 'yes' then do
   

   if exists('ram:pouetline2.txt') then
     if open(ReqF,'ram:pouetline2.txt','r') then do
       pick = readln(ReqF)
       pickstripped = substr(pick,7,length(pick)-26)

       SAY "DOWNLOAD URL IS:" pickstripped
       
       sceneorg = substr(pickstripped,1,29)
       if sceneorg == 'https://files.scene.org/view/' THEN DO
       sceneorg2 = substr(pickstripped,29)
       SAY 'Scene is 'sceneorg
       SAY 'Scene 2 is 'sceneorg2
       pickstripped  = 'https://files.scene.org/get/'sceneorg2
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
         download = 'wget --quiet -t 1 -P "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'" 'proxyaddress'/'pickstripped
         download = 'wget --quiet --user-agent="Mozilla/5.0" -t 1 -P "'DESTDIR'parties/'partystripped'/'partyyearstripped'/'reltypestripped'/'titlestripped'" 'proxyaddress'/'pickstripped
         address command download
         bs = close(ReqF)
         defaultaction=0

       END

       if extension == ".lha" THEN DO
         SAY 'This is an lha compressed file'
         download = 'wget -O ram:pouetdownload.lha 'proxyaddress'/'pickstripped
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
         download = 'wget --quiet -O ram:pouetdownload.zip 'proxyaddress'/'pickstripped
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
    open(ReqF,'pouet:pouetlastid.txt','w')
    say 'updating lastpouetid with 'pouetid+1
    writeln( ReqF, pouetid+1)
    close(ReqF)

  end
  else
  do
    SAY 'Download Failed'
  end
SAY 'response >' MyReturnCode
END /* END LOOP */
