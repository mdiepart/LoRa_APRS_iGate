# Enabling HTTPS webserver
To enable the webserver to use the HTTPS protocol, you will need to provide a pair of certificate and keys for the SSL

## OpenSSL
In order to generate certificates we will use openssl. Instructions on where to find it and how to install it can be found here: https://www.openssl.org/ or here : https://wiki.openssl.org/index.php/Binaries

## Generating a self-signed certificate
A self-signed certificate is a certificate that is not trusted (signed) by a certificate authority (CA).

1. Open a terminal from which you can execute openssl commands
2. Using the following command, generate a certificate signing request as well as an RSA key  

    ```sh
    openssl req -new -nodes -newkey rsa:2048 -sha256 -keyout prvtkey.pem -out self_signed.csr
    ```  

    You will be asked a few questions such as your country, provice,... . While answering is not mandatory, it is always a good idea to answer those as best as you can. The last two questions ("A challenge password" and "An optional company name") must be left blank.

3. Using the following command, generate the certificate and sign it with the key generated at the previous step.  
    ```sh
    openssl x509 -req -sha256 -days 3650 -in self_signed.csr -signkey prvtkey.pem -out servercert.pem
    ```  

4. Delete the file named `self_signed.csr`. It was used to generate the file `servercert.pem` and is not useful anymore.

## Modifying the configuration

1. Ensure that the generated files (`prvtkey.pem` and `servercert.pem`) are in this directory (`./ssl`).
2. Edit the file `platformio.ini` and uncomment the following lines:  
    ```ini
    #ssl/prvtkey.pem				# Uncomment this line to enable HTTPS server
 	#ssl/servercert.pem			# Uncomment this line to enable HTTPS server
    #build_flags = -DENABLE_HTTPS=1  # Uncomment this line to enable HTTPS server
    ```
    Should become:

    ```ini
    ssl/prvtkey.pem				# Uncomment this line to enable HTTPS server
 	ssl/servercert.pem			# Uncomment this line to enable HTTPS server
    build_flags = -DENABLE_HTTPS=1  # Uncomment this line to enable HTTPS server
    ```
3. Recompile the firmware and upload it to the device. The web interface should now be available via HTTPS and not HTTP.

## Security Risks

### General risks associated with HTTP

The HTTP protocol is considered as obsolete for a few years now. Using the non-secure HTTP (instead of the secure HTTPS) protocol will expose the password that you use to access the interface. Somebody having access to this interface could decide to reflash the firmware of the iGate with his own file that could be malicious. This is one of the reasons you should try to keep this interface as secure as possible (using HTTPS and a good password).

### Risks associated with self-signed certificates

A self-signed certificate is not trusted by a certificate authority. This is why your browser will probably complain the first time you connect to the HTTPS interface. This imply that somebody could place himself between the iGate and your computer, exposing the password of the interface the same way that the HTTP interface would.


