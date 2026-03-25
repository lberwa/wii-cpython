import http.server
import ssl
import socketserver
import os
import subprocess
import socket

PORT = 8000

mode = input("HTTP oder HTTPS starten? (http/https): ").strip().lower()

def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    finally:
        s.close()
    return ip

LOCAL_IP = get_local_ip()

class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            body = "Wii HTTPS/HTTP Test OK\n"
            body += "Server IP: %s\n" % LOCAL_IP
            body += "Wenn du das siehst funktioniert curl.\n"

            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(body.encode())
        else:
            self.send_error(404)

httpd = socketserver.TCPServer(("0.0.0.0", PORT), Handler)

if mode == "https":
    cert = "cert.pem"
    key = "key.pem"

    if not os.path.exists(cert) or not os.path.exists(key):
        print("Erzeuge selbstsigniertes Zertifikat...")
        subprocess.call([
            "openssl","req","-x509","-newkey","rsa:2048",
            "-keyout",key,"-out",cert,
            "-days","365","-nodes",
            "-subj","/CN=localhost"
        ])

    httpd.socket = ssl.wrap_socket(
        httpd.socket,
        keyfile=key,
        certfile=cert,
        server_side=True
    )

    print("HTTPS Server läuft")

else:
    print("HTTP Server läuft")

print("\nLokaler Zugriff:")
print("http://%s:%d" % (LOCAL_IP, PORT))

print("\nVon außen erreichbar wenn Port weitergeleitet:")
print("http://DEINE_PUBLIC_IP:%d" % PORT)

httpd.serve_forever()








"""
import http.server
import ssl
import socketserver
import os
import subprocess

PORT = 8000

mode = input("HTTP oder HTTPS starten? (http/https): ").strip().lower()

class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            body = "Wii HTTPS/HTTP Test OK\n"
            body += "Pfad: /\n"
            body += "Wenn du das siehst funktioniert curl.\n"

            self.send_response(200)
            self.send_header("Content-type", "text/plain")
            self.end_headers()
            self.wfile.write(body.encode())
        else:
            self.send_error(404)

httpd = socketserver.TCPServer(("", PORT), Handler)

if mode == "https":
    cert = "cert.pem"
    key = "key.pem"

    if not os.path.exists(cert) or not os.path.exists(key):
        print("Erzeuge selbstsigniertes Zertifikat...")
        subprocess.call([
            "openssl","req","-x509","-newkey","rsa:2048",
            "-keyout",key,"-out",cert,
            "-days","365","-nodes",
            "-subj","/CN=localhost"
        ])

    httpd.socket = ssl.wrap_socket(
        httpd.socket,
        keyfile=key,
        certfile=cert,
        server_side=True
    )

    print("HTTPS Server läuft auf Port", PORT)

else:
    print("HTTP Server läuft auf Port", PORT)

print("URL lokal:")
print("http://localhost:%d" % PORT)
print("oder im LAN:")
print("http://DEINE-IP:%d" % PORT)

httpd.serve_forever()
"""
