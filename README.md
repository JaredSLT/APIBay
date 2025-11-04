It's a really simple program, that really doesn't do much. It loads a query from apibay.org and prints it out as fast as it possibly can. This is the basis for another program of mine, TPBNab, which takes the data gathered here and converts it to Torznab XML and serves up a REST API compatible with Radarr, Sonarr, etc.

The program was designed to run on ANY computer (create an issue if you can't run it). The the first paramter is the query you wish to search for enclosed by quotes if you have spaces. As an example (remove the -v for JSON output):

```
C:\Users\user\CLionProjects\TPBSearch\cmake-build-release-clang\TPBSearch.exe -v "harry potter and the chamber of secrets"
Initialization: 169 ns
URL Escaping: 251 ns
URL Building: 47 ns
Curl Setup: 989 ns
HTTP Request: 25174576 ns
Total: 25176573 ns

Process finished with exit code 0
```

Where my ping is:
```
Pinging apibay.org [2606:4700:130:436c:6f75:6466:6c61:7265] with 32 bytes of data:
Reply from 2606:4700:130:436c:6f75:6466:6c61:7265: time=20ms
Reply from 2606:4700:130:436c:6f75:6466:6c61:7265: time=17ms
Reply from 2606:4700:130:436c:6f75:6466:6c61:7265: time=22ms
Reply from 2606:4700:130:436c:6f75:6466:6c61:7265: time=18ms

Ping statistics for 2606:4700:130:436c:6f75:6466:6c61:7265:
    Packets: Sent = 4, Received = 4, Lost = 0 (0% loss),
Approximate round trip times in milli-seconds:
    Minimum = 17ms, Maximum = 22ms, Average = 19ms
```
