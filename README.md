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

## Installation
TBD
