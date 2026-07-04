#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct LocationConfig {
    std::string path;
    std::vector<Method> methods;
    std::string root;
    std::string index;
    bool autoindex = false;
    std::string uploadStore;
    std::string cgiExtension;
    int redirectCode = 0;
    std::string redirectTarget;
};

struct ServerConfig {
    std::string host;
    int port = 0;
    std::string serverName;
    size_t clientMaxBodySize = 0;
    std::unordered_map<int, std::string> errorPages;
    std::vector<LocationConfig> locations;
};

using Config = std::vector<ServerConfig>;

/*
LocationConfig - ein "location {}" Block innerhalb eines Servers
 * ------------------------------------------------------------
 * path           - URL-Prefix, matched gegen Request-Pfad (z.B. "/", "/uploads")
 * methods        - erlaubte HTTP-Methoden fuer diese Location (GET/POST/DELETE)
 * root           - Dateisystem-Pfad, der an path drangehaengt wird beim Serving
 * index          - Default-Datei, die bei Verzeichnis-Requests ausgeliefert wird (z.B. index.html)
 * autoindex      - true = Verzeichnislisting anzeigen, wenn kein index gefunden wird | false = errorpage
 * uploadStore    - Zielordner fuer POST-Uploads (nur relevant wenn POST erlaubt)
 * cgiExtension   - Datei-Endung, die ans CGI weitergereicht wird (z.B. ".php")
 * redirectCode   - HTTP-Statuscode fuer Redirect (301/307), 0 = kein Redirect konfiguriert
 * redirectTarget - Ziel-URL fuer den Redirect
 *
 * ServerConfig - ein "server {}" Block, entspricht einer eigenstaendigen Webseite
 * ------------------------------------------------------------
 * host               - IP-Adresse zum Binden (z.B. "0.0.0.0")
 * port               - Port zum Binden (listen-Direktive)
 * serverName         - server_name, genutzt fuer virtuelle Hosts (Host-Header-Matching)
 * clientMaxBodySize  - maximale Request-Body-Groesse in Bytes (aus "1m"/"512k" umgerechnet)
 * errorPages         - Mapping HTTP-Code -> Pfad zur eigenen Error-Page (z.B. 404 -> /errors/404.html)
 * locations          - alle location-Bloecke dieses Servers, Reihenfolge fuers Matching wichtig
 *
 * Config - ganze Config-Datei = Liste von ServerConfig (mehrere "server {}" Bloecke moeglich)
 */
