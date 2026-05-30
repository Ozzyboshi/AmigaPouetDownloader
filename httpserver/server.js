var http = require('http');
var https = require('https');
var url = require('url');

http.createServer(function (req, resp) 
{
  var h = req.headers;
  h['user-agent'] = "Mozilla/6.0"
  
  downloadurl=req.url.substring(1);
  downloadpath=url.parse(downloadurl).pathname;
  downloadhost=url.parse(downloadurl).host;
  downloadparams=url.parse(downloadurl).query;
  console.log("Request url:"+downloadurl);
  console.log("Download path:"+downloadpath);
  console.log("Download host:"+downloadhost);
  console.log("Download params:"+downloadparams);
  if (downloadparams!=null && downloadparams!="") downloadpath=downloadpath+"?"+downloadparams;
  downloadpath = downloadpath.replace(/&amp;/g, "&"); // Corregge l'errore

  console.log("Download path with query params after encoding:"+downloadpath);
  if (downloadurl.startsWith("http://"))
  {
    console.log("Downloading from non https server");
    h.host=downloadhost;
    var req2 = http.request({ host: h.host, port: 80, path: downloadpath, method: req.method, headers: h }, function (resp2) 
    {
      if (resp2.statusCode == "302" || resp2.statusCode == "301" || resp2.statusCode == "308" )
      {
        console.log("Redirect found");
        downloadurl = resp2.headers['location'];
        downloadpath=url.parse(downloadurl).pathname;
        if (url.parse(downloadurl).query!=null) downloadpath=downloadpath+"?"+url.parse(downloadurl).query;
        downloadhost=url.parse(downloadurl).host;
        h.host=downloadhost;
        console.log("New Request url:"+downloadurl);
        console.log("New Download path:"+downloadpath);
        resp.writeHead(resp2.statusCode, resp2.headers);
        var req3 = https.request({ host: h.host, port: 443, path: downloadpath, method: req.method, headers: h, rejectUnauthorized: false }, function (resp3) 
        {
          resp.writeHead(resp3.statusCode, resp3.headers);
          resp3.on('data', function (d) { resp.write(d); });
          resp3.on('end', function () { resp.end(); });
        });
        req3.on('error', function (err) {
          console.error("HTTPS req3 error (http->redirect): " + err.message);
          if (!resp.headersSent) resp.writeHead(502);
          resp.end("Proxy error: " + err.message);
        });
        resp2.on('data', function (d) { req3.write(d); });
        resp2.on('end', function () { req3.end(); });
      }
      else
      {
        resp.writeHead(resp2.statusCode, resp2.headers);
        resp2.on('data', function (d) { resp.write(d); });
        resp2.on('end', function () { resp.end(); });
      }
    });
    req2.on('error', function (err) {
      console.error("HTTP req2 error: " + err.message);
      if (!resp.headersSent) resp.writeHead(502);
      resp.end("Proxy error: " + err.message);
    });
    req.on('data', function (d) { req2.write(d); });
    req.on('end', function () { req2.end(); });
  }
  else
  {
    console.log("Downloading from https server");
    h.host=downloadhost;
    var req2 = https.request({ host: h.host, port: 443, path: downloadpath, method: req.method, headers: h, rejectUnauthorized: false }, function (resp2) 
    {
      if (resp2.statusCode == "302" || resp2.statusCode == "301" || resp2.statusCode == "308" )
      {
        console.log("Redirect found");
        downloadurl = resp2.headers['location'];
        downloadpath=url.parse(downloadurl).pathname;
        if (url.parse(downloadurl).query!=null) downloadpath=downloadpath+"?"+url.parse(downloadurl).query;
        downloadhost=url.parse(downloadurl).host;
        h.host=downloadhost;
        console.log("New Request url:"+downloadurl);
        console.log("New Download path:"+downloadpath);
        resp.writeHead(resp2.statusCode, resp2.headers);
        var req3 = https.request({ host: h.host, port: 443, path: downloadpath, method: req.method, headers: h, rejectUnauthorized: false }, function (resp3) 
        {
          resp.writeHead(resp3.statusCode, resp3.headers);
          resp3.on('data', function (d) { resp.write(d); });
          resp3.on('end', function () { resp.end(); });
        });
        req3.on('error', function (err) {
          console.error("HTTPS req3 error (https->redirect): " + err.message);
          if (!resp.headersSent) resp.writeHead(502);
          resp.end("Proxy error: " + err.message);
        });
        resp2.on('data', function (d) { req3.write(d); });
        resp2.on('end', function () { req3.end(); });
      }
      else
      {
        resp.writeHead(resp2.statusCode, resp2.headers);
        resp2.on('data', function (d) { resp.write(d); });
        resp2.on('end', function () { resp.end(); });
      }
    });
    req2.on('error', function (err) {
      console.error("HTTPS req2 error: " + err.message);
      if (!resp.headersSent) resp.writeHead(502);
      resp.end("Proxy error: " + err.message);
    });
    req.on('data', function (d) { req2.write(d); });
    req.on('end', function () { req2.end(); });
  }
}).listen(9999, "0.0.0.0");