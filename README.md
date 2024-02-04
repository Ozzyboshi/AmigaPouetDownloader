# AmigaPouetDownloader
Pouet scraper on arexx for classic amigas

## What is it for?
AmigaPouetDownloader it's a little arexx script that allows you to download and organize entries from http://pouet.net website on your Amiga. 

The main goal is to feed the arexx script with a pouet id entry (or a range) and it will go through one by one, detect if it's an Amiga release and if yes download it on your local Amiga file system. 

After downloading, the script will also check what type of content it is (is it a zipfile? a lha archive? an adf?) and will try to uncompress in a specific folder under pouet: assignment. 

For example, let's assume the arexx script finds a demo called "release.lha" which has been released at partyxxx of 2023, in this case the directory
_pouet:party/partyxxx/2023/demos/release_ 
will be created and file _release.lha_ will be copied and uncompressed inside it.

This should help to keep your Amiga hard drive well organized while saving you the hassle to go manually with your Amiga browser on pouet, download file, create directories and uncompress.

## How does it work?
The arexx script just uses wget utility to get data from pouet and to download the release file. 

Unfortunately, from my experience, wget for Amiga struggles to get encrypted data from the internet, especially when TLS is used. 

I tried to automate Ibrowse to perform this job but still Ibrowse requires some user interaction to select the destination directory and since I want a fully automated scraper this is a deal breaker for me. 

Hopefully with Ibrowse newer version I would be able to replace wget with Ibrowse, who knows? 

The solution of the https problem is quite ugly but it works, I created sort of "http proxy" on nodejs, it's not really a proxy but a kind of. 

What it does it gets a regular unencrypted HTTP request which contains the HTTPS url of the file the Amiga wants, then it goes to the internet to get the HTTPS data and responds with and HTTP response back to the Amiga. 

This works fine but you requires a modern machine with nodejs, so... not neat solution but since I have a spare server at home it works for me.

You can find in this repository the docker stuff to get the nodejs up and running and the Amiga arexx script.

## Requirements
- A x86/x64 PC connected to the internet with Docker+Docker composer installed (tested with Ubuntu)
- A classic amiga with 1 meg of ram, hard drive and a connection to the internet + tcp/ip stack.

The PC and the Amiga must be able to talk at IP level, in particular the Amiga must be able to reach the PC at the port specified inside the docker-compose.yml (default is port 9999).
WARNING!!!! If possible dont use a public in cloud machine, it would be better to secure the PC in a private network since there are no security mechanism to prevent unauthorized download access.

These are my setups where this script has been succesfully tested:
- A500 plus + Indivision ACA500Plus - wb3.1 - 1MB of Chip - Plipbox + Roadshow
- A600 with Vampire V2 + CoffinOS + PCMCIA network card + Roadshow
- A600 with Pistorm 600 + CaffeineOS + Plipbox + MiamiDX

## Installation
### PC side
```
$ git clone https://github.com/Ozzyboshi/AmigaPouetDownloader
Cloning into 'AmigaPouetDownloader'...
remote: Enumerating objects: 29, done.
remote: Counting objects: 100% (29/29), done.
remote: Compressing objects: 100% (19/19), done.
remote: Total 29 (delta 9), reused 24 (delta 6), pack-reused 0
Unpacking objects: 100% (29/29), done.
Checking connectivity... fatto.

$ cd AmigaPouetDownloader/
$ docker-compose build
Building proxy
Step 1/7 : FROM node:16
 ---> bfb7b2a05614
Step 2/7 : WORKDIR /usr/src/app
 ---> Using cache
 ---> 2f556a748124
Step 3/7 : COPY package*.json ./
 ---> c8f3405eb1dd
Step 4/7 : RUN npm install
 ---> Running in 0dca63830f8e
Removing intermediate container 0dca63830f8e
 ---> 106e5ded99df
Step 5/7 : COPY . .
 ---> 11d5dd62b034
Step 6/7 : EXPOSE 9999
 ---> Running in bb1b68c3031a
Removing intermediate container bb1b68c3031a
 ---> ab65dcab4924
Step 7/7 : CMD [ "node", "server.js" ]
 ---> Running in 09c0d6421c6d
Removing intermediate container 09c0d6421c6d
 ---> 81764a4e092b

Successfully built 81764a4e092b
Successfully tagged ozzyboshi/pouetdownloader:latest

$ docker-compose up -d
OR
$ docker run -d -p 9999 ozzyboshi/pouetdownloader
c9dbaebfab063efe25b74d2ae6b70bd3a0a321dc74207b5318cc17f36ec1cbfd


```

### Amiga side
Installation on the amiga is a little bit longer but it's a one time process, let's go through all necessary
- copy the file pouet.rexx in this repository under directory "amiga" into rexx: of your real Amiga.
- Download http://aminet.net/util/sys/mkdir13.lha into your amiga, uncompress it and copy file mkdir to c:, this is necessary since I cant rely on mkdir command from amigaos since the syntax changes from os to os.
- Open s:startup-sequence and add the following string
 ```
 Assign >NIL: POUET: RAM:
 ```
 This is the root directory where the downloader will store data from pouet, of course change RAM: with the path you want to use.
 Also add the following line to the startup.sequence
 ```
 SetEnv pouetproxy x.x.x.x:9999
 ```
Where x.x.x.x is the ipv4 adress of your PC running the proxy and 9999 is the port.

Make sure *wget* and *grep* are installed and in path, this tools are preinstalled if you are using CoffinOS or Caffeine but they dont come out of the box if you are using old Workbenches like OS3.1. 

The *wget* utility is usually included with the plipbox network boot disk, I think Roadshow also provides his own version. 

If you dont have them, aminet is a good place where to find it but keep in mind to use a version compatible with your processor (old stock 68000 or 68020+).

Also install :
 - lha (https://aminet.net/package/util/arc/lha)
 - unzip (http://aminet.net/package/util/arc/UnZip)
 - xdms (http://aminet.net/package/util/arc/xDMS)

to allow pouetdownloader to decompress lha, zipfile and dms files once they are downloaded.

### Usage
Open your CLI (or SHELL) and type:
```
rx pouet [<id>] [id2]
```
If you dont provide any argument, the script will try to detect the last downloaded release and it will resume from there. This feature in order to work needs the file pouet:pouetlastid.txt previously created with the last pouet id inside.
Id is the id you find in the pouet url of your release, for example, if you want to download this release:
```
https://www.pouet.net/prod.php?which=93426
```

you have to type

```
rx pouet 93426
```

If you add another id, pouet downloader will try to download all releases from id1 until id2.
Non Amiga related releases will be discarded.
