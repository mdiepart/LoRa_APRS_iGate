Detailed instructions are yet to come.

Basically you have to
1) generate a private key and a server certificate for the HTTPS server.
2) Place them in this folder as `prvtkey.pem` and `servercert.pem`
3) Uncomment the following lines in `platformio.ini`
```
board_build.embed_txtfiles =
 ssl/prvtkey.pem
 ssl/servercert.pem
build_flags = -DENABLE_HTTPS
```
4) Compile the project and upload the firmware. The webserver should now be available via HTTPS (and not HTTP).
