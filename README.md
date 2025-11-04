It's a really simple program, that really doesn't do much. It loads a query from apibay.org and prints it out as fast as it possibly can. This is the basis for another program of mine, TPBNab, which takes the data gathered here and converts it to Torznab XML and serves up a REST API compatible with Radarr, Sonarr, etc.

The program was designed to run on ANY computer (create an issue if you can't run it). Build with clang for the best performance. The the first paramter is the query you wish to search for enclosed by quotes if you have spaces. As an example (remove the -v for JSON output):

```
C:\Users\user\CLionProjects\TPBSearch\cmake-build-release-clang\TPBSearch.exe -v "harry potter and the chamber of secrets"
Initialization: 3413561 ns
URL Escaping: 448 ns
URL Building: 170 ns
Curl Setup: 14 ns
HTTP Request: 149965079 ns
Total: 153381456 ns

Process finished with exit code 0
```

```
C:\Users\user\CLionProjects\TPBSearch\cmake-build-release-clang\TPBSearch.exe "harry potter and the chamber of secrets"
[{"id":"7381763","name":"Harry Potter and the Chamber of Secrets (2002) 1080p BrRip x264 ","info_hash":"8D172817F4B39A12
B07F64DFA2F3544B89772635","leechers":"277","seeders":"446","num_files":"6","size":"2206526005","username":"YIFY","added"
:"1340522230","status":"vip","category":"207","imdb":"tt0295297"},{"id":"5580482","name":"Harry Potter and the Chamber o
f Secrets (2002)720p- 600mb- YIFY","info_hash":"B6DC52BD753674B57B143A515CD871B5027F0522","leechers":"28","seeders":"88"
,"num_files":"1","size":"628979934","username":"YIFY","added":"1274516108","status":"vip","category":"207","imdb":"tt029
5297"},{"id":"34721263","name":"Harry.Potter.and.the.Chamber.of.Secrets.2002.2160p.UHD.BluRay.10","info_hash":"E393FEBDD
56C42B6DE8BC9B17CEBE0FCD685AFA0","leechers":"48","seeders":"67","num_files":"2","size":"19132457337","username":"GoodFil
ms","added":"1569421406","status":"vip","category":"211","imdb":""},{"id":"79500355","name":"Harry Potter And The Chambe
r Of Secrets 2002 PROPER EXTENDED 1080p BluRay x","info_hash":"9E479C8FF09CE87A4CBAD11440CF83B01C8B02F1","leechers":"16"
,"seeders":"44","num_files":"4","size":"6672514015","username":"TvTeam","added":"1749867400","status":"vip","category":"
207","imdb":""},{"id":"5268795","name":"Harry Potter And The Chamber Of Secrets [PC-Game]","info_hash":"639F9D7C48CF91A7
6E71BB959CD97C9019A7B1C5","leechers":"0","seeders":"25","num_files":"5","size":"644535432","username":"eyezin","added":"
1262974869","status":"vip","category":"401","imdb":""},{"id":"27730202","name":"Harry.Potter.And.The.Chamber.of.Secrets.
2002.EXTENDED.1080p.BluR","info_hash":"D308DEEA301AC6F21FCEB01ACE552A294093AC2D","leechers":"7","seeders":"22","num_file
s":"5","size":"14180972255","username":"GoodFilms","added":"1545478199","status":"vip","category":"207","imdb":""},{"id"
:"79247921","name":"Harry Potter and the Chamber of Secrets 2002 Extended Cut 1080p BluRay DTS ","info_hash":"B9C81EFFD0
C27BA974F28DE797EC1E186DC102A0","leechers":"16","seeders":"19","num_files":"4","size":"18285018742","username":"TvTeam",
"added":"1747948540","status":"vip","category":"207","imdb":""},{"id":"80669236","name":"Harry Potter and the Chamber of
 Secrets 2002 1080p ITV WEB-DL AAC 2 0 H 264","info_hash":"2F4D6362ED59E99237DD2960BA898A872994FA84","leechers":"9","see
ders":"19","num_files":"3","size":"7127248098","username":"TvTeam","added":"1759192197","status":"vip","category":"207",
"imdb":""},{"id":"67538124","name":"Harry.Potter.And.The.Chamber.of.Secrets.2002.EXTENDED.1080p.BluR","info_hash":"0E189
3FDCBB1587A1D5A1487E707E8B3CD821A5D","leechers":"5","seeders":"17","num_files":"4","size":"3572297317","username":"TGxGo
odies","added":"1680770605","status":"vip","category":"207","imdb":"tt0295297"},{"id":"79480220","name":"Harry Potter An
d The Chamber Of Secrets 2002 EXTENDED 1080p BluRay x265-YAW","info_hash":"A1250E676F0A13C5E7A93560296EA4AA0AB2A255","le
echers":"17","seeders":"14","num_files":"4","size":"5007549793","username":"TvTeam","added":"1749706692","status":"vip",
"category":"207","imdb":""},{"id":"80301376","name":"Harry Potter and the Chamber of Secrets 2002 720p MAX WEB-DL DDP5 1
 H 264 D","info_hash":"A8F4CAFF171B4B033D8C258FDAAED695D02242EA","leechers":"4","seeders":"12","num_files":"3","size":"2
265966866","username":"TvTeam","added":"1756199354","status":"vip","category":"207","imdb":""},{"id":"78512303","name":"
Harry Potter and the Chamber of Secrets 2002 1080p PCOK WEB-DL DDP 5 1 H 26","info_hash":"4F19BBF786D0385BF09514EE319803
201E0AFBBB","leechers":"1","seeders":"5","num_files":"3","size":"9960956832","username":"TvTeam","added":"1742263431","s
tatus":"vip","category":"207","imdb":""},{"id":"78512623","name":"Harry Potter And The Chamber Of Secrets 2002 1080p PCO
K WEB-DL H264-PiRaTeS","info_hash":"3E49B554395C15739CBC50132A7A4629EBCF4CB9","leechers":"50","seeders":"5","num_files":
"3","size":"9960956823","username":"TvTeam","added":"1742266064","status":"vip","category":"207","imdb":""},{"id":"79982
166","name":"Harry Potter and the Chamber of Secrets 2002 1080p BluRay DDP 5 1 10bit H 2","info_hash":"A924163817697B078
12945B65A4F69598B17BEE6","leechers":"2","seeders":"5","num_files":"3","size":"3522396735","username":"TvTeam","added":"1
753662851","status":"vip","category":"207","imdb":""},{"id":"27529808","name":"Harry.Potter.and.the.Chamber.of.Secrets.2
002.1080p.BluRay.x264-S","info_hash":"DAF8B9C1C7F729407F2035124965B29C366389F9","leechers":"3","seeders":"4","num_files"
:"2","size":"15293924189","username":"GoodFilms","added":"1545160130","status":"vip","category":"207","imdb":"tt0295297"
},{"id":"80649666","name":"Harry Potter and the Chamber of Secrets 2002 1080p AMZN WEB-DL H265 SDR DDP","info_hash":"95B
7638CFDEA7AFDA4F3565F43764731DCB8EACF","leechers":"4","seeders":"4","num_files":"3","size":"5970580405","username":"TvTe
am","added":"1759036712","status":"vip","category":"207","imdb":""},{"id":"3988564","name":"Harry Potter And The Chamber
 Of Secrets","info_hash":"94D2B78AD097C72918CC646AFE33BAE3898EC63B","leechers":"0","seeders":"3","num_files":"1","size":
"265695586","username":"captainelliotspencer","added":"1200930558","status":"vip","category":"401","imdb":""},{"id":"282
17223","name":"Harry.Potter.and.the.Chamber.of.Secrets.2002.Multi.2160p.HDR.x26","info_hash":"C91FFE8E80FE850D11AFFD3BE9
5A3B940B092929","leechers":"1","seeders":"3","num_files":"3","size":"22722259760","username":"SaM","added":"1546631886",
"status":"vip","category":"211","imdb":""},{"id":"79125204","name":"Harry Potter and the Chamber of Secrets 2002 1080p B
luRay x264-OFT","info_hash":"EE1B8DD1F66FC2261757B97659EF9972DA898A7F","leechers":"4","seeders":"3","num_files":"4","siz
e":"7494412874","username":"TvTeam","added":"1746991164","status":"vip","category":"207","imdb":""},{"id":"8934118","nam
e":"Harry Potter and the Chamber of Secrets (2002) Extended 1080p.BR","info_hash":"F3731315CC2FF2F686ADB5848E61497B3112A
019","leechers":"3","seeders":"2","num_files":"6","size":"2199968127","username":"sujaidr","added":"1379515731","status"
:"vip","category":"207","imdb":"tt0295297"},{"id":"10064356","name":"Harry Potter and the Chamber of Secrets 2002 1080p
 - Ozlem","info_hash":"EEB23FC9CDF5632B000451FAE090BD6E646EC74E","leechers":"1","seeders":"2","num_files":"1","size":"27
54920856","username":"e.ozlem","added":"1398770253","status":"vip","category":"207","imdb":"tt0295297"},{"id":"16568433"
,"name":"Harry Potter and the Chamber of Secrets (2002) HQ 1080p Blu-Ray ","info_hash":"48ACF523B0CC3427AF14F87EE2369FDC
06ABB427","leechers":"0","seeders":"2","num_files":"4","size":"13831942424","username":"SaM","added":"1482420154","statu
s":"vip","category":"207","imdb":""},{"id":"5420811","name":"Harry Potter And The Chamber Of Secrets [PS2] [NTSC]","info
_hash":"D475BEAB50951AEDBA8840C098A2B1CC5A24AA8E","leechers":"0","seeders":"1","num_files":"1","size":"510682543","usern
ame":"thenoobish","added":"1268078182","status":"vip","category":"403","imdb":""},{"id":"6426046","name":"Harry Potter A
nd The Chamber of Secrets 2002.720p.BRRip.HDLiTE","info_hash":"F6983982335DAA362AE3FF411BDD2ECCFA5D802C","leechers":"0",
"seeders":"1","num_files":"3","size":"3487082797","username":"neon","added":"1306468973","status":"vip","category":"207"
,"imdb":"tt0295297"},{"id":"8740139","name":"Harry Potter And The Chamber Of Secrets 2002 1080p BrRip x264-Th","info_has
h":"3D13443FE46A16AF9C10036D33E54C1D4E41DADF","leechers":"0","seeders":"1","num_files":"4","size":"2459515201","username
":"ThumperTM","added":"1374754968","status":"vip","category":"207","imdb":"tt0295297"},{"id":"8742909","name":"Harry Pot
ter And The Chamber Of Secrets 2002 720p BrRip x264-Thu","info_hash":"B8BF4E5D768A473779BF3BCF18645C4A3C0C3AC1","leecher
s":"0","seeders":"1","num_files":"4","size":"1545683814","username":"ThumperTM","added":"1374832188","status":"vip","cat
egory":"207","imdb":"tt0295297"},{"id":"10064430","name":"Harry Potter and the Chamber of Secrets 2002 720p - Ozlem","in
fo_hash":"F9D50706EFFBB99EAD4BF1F95C4345B53C522A58","leechers":"1","seeders":"1","num_files":"1","size":"1381693055","us
ername":"e.ozlem","added":"1398771274","status":"vip","category":"207","imdb":"tt0295297"},{"id":"10185892","name":"Harr
y Potter and the Chamber of Secrets Extended Edition 2002 72","info_hash":"25960F88196E10ED8069566BA8203996E2DB715E","le
echers":"0","seeders":"1","num_files":"4","size":"9222441393","username":"Drarbg","added":"1400574911","status":"vip","c
ategory":"201","imdb":"tt0295297"},{"id":"10185895","name":"Harry Potter and the Chamber of Secrets Extended Edition 200
2 10","info_hash":"A23879482FD3A0ECF20E336FB9B86BDF80610D43","leechers":"1","seeders":"1","num_files":"4","size":"199636
34393","username":"Drarbg","added":"1400574969","status":"vip","category":"201","imdb":"tt0295297"},{"id":"10945890","na
me":"Harry.Potter.And.The.Chamber.Of.Secrets.2002.DVDRip.x264.AC3-iCM","info_hash":"5D0394EEAE110D06251834B5D4146F605141
7A20","leechers":"0","seeders":"1","num_files":"4","size":"1072488185","username":"TvTeam","added":"1409589951","status"
:"vip","category":"201","imdb":""},{"id":"12491859","name":"Harry Potter and the Chamber of Secrets 2002 BDRip 720p","in
fo_hash":"1E79E1B1E16A880CBDE1C81C0144441642BC2947","leechers":"0","seeders":"1","num_files":"9","size":"1254046981","us
ername":"Iznogoud9","added":"1443178980","status":"vip","category":"207","imdb":"tt0295297"},{"id":"12491885","name":"Ha
rry Potter and the Chamber of Secrets 2002 BDRip 1080p","info_hash":"EB46DF828DD8078DC7C6BC319E95F4599F68FACA","leechers
":"1","seeders":"1","num_files":"9","size":"4160252778","username":"Iznogoud9","added":"1443179794","status":"vip","cate
gory":"207","imdb":"tt0295297"},{"id":"20863763","name":"Harry.Potter.and.the.Chamber.of.Secrets.2002.MULTi.1080p.Blu-ra
y","info_hash":"37F7DA2574B786C5A8039F5A60576F615774027C","leechers":"25","seeders":"1","num_files":"4","size":"61922155
97","username":"SaM","added":"1522597938","status":"vip","category":"207","imdb":""},{"id":"77235320","name":"Harry Pott
er and the Chamber of Secrets 2002 Extended 1080p BluRay x264-OFT","info_hash":"8451160B32C379980410C9F9D24AFE0A41043BAE
","leechers":"12","seeders":"1","num_files":"4","size":"8122191719","username":"TvTeam","added":"1731834578","status":"v
ip","category":"207","imdb":""},{"id":"3741767","name":"Harry.Potter.And.The.Chamber.Of.Secrets.2002.PAL.DVDR-TFT","info
_hash":"81152571FBDBA8111170F26158A4D71567B474A3","leechers":"1","seeders":"0","num_files":"1","size":"4614076416","user
name":"He0n","added":"1184414058","status":"vip","category":"202","imdb":"tt0295297"},{"id":"5098007","name":"Harry Pott
er And The Chamber Of Secrets 2002 1080p BD9 x264-IGUA","info_hash":"61104070B46D34B0615A9CD9F075FBAF12A3E31E","leechers
":"1","seeders":"0","num_files":"88","size":"8485922383","username":"BOZX","added":"1253729042","status":"vip","category
":"207","imdb":"tt0295297"},{"id":"5390252","name":"Harry Potter and the Chamber of Secrets [HD-Rip ITA ENG - Sub It","i
nfo_hash":"FD5B637915F1F6190A9DA03DB188E780E1845396","leechers":"1","seeders":"0","num_files":"2","size":"9067172307","u
sername":"Colombo-bt","added":"1267036466","status":"vip","category":"207","imdb":""},{"id":"6510734","name":"Harry.Pott
er.and.the.Chamber.of.Secrets.2002.x264.DTS.4AUDIO-WAF","info_hash":"3316551DB4DB950DAE10BC79DFE01D64B1B91536","leechers
":"1","seeders":"0","num_files":"7","size":"5138325559","username":"sadbawang","added":"1309604791","status":"vip","cate
gory":"201","imdb":"tt0295297"},{"id":"6658639","name":"Harry.Potter.And.The.Chamber.Of.Secrets.2002.WS.WS.DVDRip.XViD.i
","info_hash":"D486B1B700EDECF9517B6F204D2F79AB5054DD22","leechers":"0","seeders":"0","num_files":"11","size":"757280162
","username":"TvTeam","added":"1315415849","status":"vip","category":"201","imdb":"tt0295297"},{"id":"7654667","name":"H
arry.Potter.And.The.Chamber.Of.Secrets.2002.iNTERNAL.DVDRip.Xvi","info_hash":"184E81C64FF685F450943530AB095C1A7EDAD6A7",
"leechers":"1","seeders":"0","num_files":"5","size":"1467131664","username":"TvTeam","added":"1348112303","status":"vip"
,"category":"201","imdb":""},{"id":"7710101","name":"Harry Potter and the Chamber of Secrets [XBOX360] [PAL]","info_hash
":"3AA07101F222BCE61E5CF0058F965E69EA21BE5C","leechers":"1","seeders":"0","num_files":"2","size":"1914425920","username"
:"thenoobish","added":"1349779508","status":"vip","category":"404","imdb":""},{"id":"7737486","name":"Harry.Potter.And.T
he.Chamber.Of.Secrets.2002.iNT.DVDRip.x264-8Ba","info_hash":"D6B6C6EEF3F5576755E997EBD78AA31E3ECEE79C","leechers":"1","s
eeders":"0","num_files":"5","size":"2096647788","username":"TvTeam","added":"1350516378","status":"vip","category":"201"
,"imdb":""},{"id":"8459234","name":"Harry.Potter.And.The.Chamber.of.Secrets.2002.EXTENDED.600MB.BRRi","info_hash":"21DFE
0BD573415A7CEF89E09EC3D3991CFFA9711","leechers":"1","seeders":"0","num_files":"3","size":"629290005","username":"TvTeam"
,"added":"1368184380","status":"vip","category":"201","imdb":""},{"id":"9438173","name":"Harry Potter and the Chamber of
 Secrets (2002) BluRay 720p x264 ","info_hash":"3E839A88BF312A3D49DAECD8227C134CD5076F69","leechers":"1","seeders":"0","
num_files":"1","size":"1198575458","username":".BONE.","added":"1388941629","status":"vip","category":"207","imdb":"tt02
95297"},{"id":"9863582","name":"Harry.Potter.And.The.Chamber.of.Secrets.2002.EXTENDED.iNTERNAL.B","info_hash":"F72CF1E45
C953CC1573293E3034710A5067DC855","leechers":"0","seeders":"0","num_files":"4","size":"1208898787","username":"TvTeam","a
dded":"1396309972","status":"vip","category":"201","imdb":""},{"id":"9864876","name":"Harry Potter And The Chamber of Se
crets 2002 EXTENDED iNTERNAL B","info_hash":"F4857F934ADD81FB2DF8B8266140B3FAF0B50F51","leechers":"0","seeders":"0","num
_files":"31","size":"1222318108","username":"hotpena","added":"1396330081","status":"vip","category":"201","imdb":"tt029
5297"},{"id":"12491808","name":"Harry Potter and the Chamber of Secrets 2002 BDRip 480p","info_hash":"4CD3BC31E7BA16DE01
B3573AA359C1D10CF92183","leechers":"1","seeders":"0","num_files":"9","size":"595506470","username":"Iznogoud9","added":"
1443178053","status":"vip","category":"201","imdb":"tt0295297"},{"id":"36385483","name":"Harry.Potter.And.The.Chamber.Of
.Secrets.2002.WS.DVDRip","info_hash":"1D9B80868261542E3166E83E2EDAAE934EA693AF","leechers":"0","seeders":"0","num_files"
:"6","size":"1464812828","username":"TvTeam","added":"1597157336","status":"vip","category":"201","imdb":"tt0295297"},{"
id":"9466816","name":"J. K. Rowling - Harry Potter and the Chamber of Secrets - Epub","info_hash":"95657B635E573F1207D2B
9E43B74DB2299300BCC","leechers":"0","seeders":"43","num_files":"1","size":"1188665","username":"book1001","added":"13894
66862","status":"trusted","category":"601","imdb":""},{"id":"74893401","name":"Harry.Potter.And.The.Chamber.Of.Secrets.2
002.1080p.ENG.LATINO.DD.5.1.H264.BTM","info_hash":"890DBEC8D6E03C039BA7DF37C81B3348F934C223","leechers":"11","seeders":"
24","num_files":"4","size":"10872513897","username":"Freddy1714","added":"1710984296","status":"trusted","category":"207
","imdb":"tt0295297"},{"id":"4390640","name":"Harry Potter and the Chamber of Secrets Audio Book","info_hash":"CC9F66587
6F7E3968D78711D1AABEAAD69A2BAD7","leechers":"1","seeders":"1","num_files":"175","size":"532841315","username":"b0473896"
,"added":"1221151025","status":"trusted","category":"102","imdb":""},{"id":"6018956","name":"Harry Potter and the chambe
r of secrets 2002 Hindi-Eng 720p~3385","info_hash":"7FA879BD0C7E687DDCC45A88FCFBC5CCD3803CE6","leechers":"0","seeders":"
1","num_files":"3","size":"2603054783","username":"3385VKSH","added":"1291729654","status":"trusted","category":"207","i
mdb":"tt0295297"},{"id":"6286265","name":"Harry.Potter.2 - Harry Potter and the Chamber of Secrets - Arabi","info_hash":
"1B3B480EE9CF04ACB6ED597C9DE50432B09A246C","leechers":"1","seeders":"1","num_files":"1","size":"1584013608","username":"
samir_ar","added":"1301619143","status":"trusted","category":"201","imdb":"tt0295297"},{"id":"7812045","name":"Harry Pot
ter and The Chamber Of Secrets 2001 + Crack [UJ.rip]","info_hash":"784418EA59036D4A7D466A34F8DEF9D7D579469A","leechers":
"0","seeders":"1","num_files":"185","size":"716737300","username":"AshUtpal","added":"1352556736","status":"trusted","ca
tegory":"401","imdb":""},{"id":"18191222","name":"Harry Potter and the Chamber of Secrets(2002)MPEG-4[DaScubaDude]","inf
o_hash":"510F7E066CD95A7DEC4B78BB555F78AF5A61814B","leechers":"0","seeders":"1","num_files":"1","size":"1609679260","use
rname":"dascuba","added":"1500743228","status":"trusted","category":"201","imdb":"tt0295297"},{"id":"56371955","name":"H
arry Potter and the Chamber of Secrets 1080p BluRay x264 MIKY","info_hash":"AED3D9906BF8D8B9D624969F0363E68F6195414D","l
eechers":"0","seeders":"1","num_files":"1","size":"4268800124","username":"mike953100","added":"1643888864","status":"tr
usted","category":"207","imdb":""},{"id":"70675189","name":"Harry Potter And The Chamber of Secrets (2002) [1080p]","inf
o_hash":"44C499D0FD95AD14DE1A43B84DDF486BC3C2A4E3","leechers":"7","seeders":"1","num_files":"1","size":"3124678025","use
rname":"Anonymous","added":"1690196983","status":"trusted","category":"207","imdb":"tt0295297"},{"id":"4141462","name":"
Harry Potter And The Chamber Of Secrets 2002 x264 DTS HD DVDRip","info_hash":"35E70E4BC4E0C82F108C005EA7AAF499CF76CB02",
"leechers":"0","seeders":"0","num_files":"3","size":"8549704620","username":"SpagMax","added":"1208511415","status":"tru
sted","category":"207","imdb":"tt0295297"},{"id":"4607518","name":"Harry Potter and the Chamber of Secrets[SWESUB][2002]
-TheOne","info_hash":"7E26BDA33C0164B98843CD25281A8BAAD06D6AE1","leechers":"0","seeders":"0","num_files":"2","size":"146
7977728","username":"kukhuvebarn","added":"1230517096","status":"trusted","category":"201","imdb":""},{"id":"4868866","n
ame":"(PSX-PSP) Harry Potter and The Chamber of Secrets converted prop","info_hash":"2B84FEFB7C1001E7CEE78F00EF722D3026E
F3718","leechers":"0","seeders":"0","num_files":"2","size":"530947014","username":"toxicjuggalo","added":"1240674642","s
tatus":"trusted","category":"406","imdb":""},{"id":"5001408","name":"Harry Potter and the Chamber of Secrets (Book 2) [O
sheraj]","info_hash":"4D21698977D971D71E65CE6FD129FDAAEBBD47AF","leechers":"1","seeders":"0","num_files":"3","size":"150
741","username":"Osheraj","added":"1247527373","status":"trusted","category":"601","imdb":""},{"id":"5033571","name":"HA
RRY POTTER AND THE CHAMBER OF SECRETS [Osheraj]","info_hash":"24E37721996CDBA6936FA549DD26D882A3002C96","leechers":"0","
seeders":"0","num_files":"1","size":"655308","username":"Osheraj","added":"1248998126","status":"trusted","category":"60
1","imdb":""},{"id":"5037122","name":"HARRY POTTER AND THE CHAMBER OF SECRETS [Osheraj]","info_hash":"2EBB6466244388B37A
936E95E10B57B190737882","leechers":"1","seeders":"0","num_files":"1","size":"655308","username":"Osheraj","added":"12491
65162","status":"trusted","category":"601","imdb":""},{"id":"6010961","name":"Harry Potter And The Chamber Of Secrets 20
02 BRRip 1080p x264 AA","info_hash":"F188490D9F386D412AD1FB933ECDFFE30EA8E1A3","leechers":"0","seeders":"0","num_files":
"7","size":"4037306465","username":"honchorella31","added":"1291452695","status":"trusted","category":"207","imdb":"tt02
95297"},{"id":"6322964","name":" Harry.Potter.and.the.Chamber.of.Secrets.2002.BluRay.1080p.DTS.x","info_hash":"633BFA25F
7D0FBEF88A4C472114FAD38CD10D0F4","leechers":"1","seeders":"0","num_files":"3","size":"9922956570","username":"jakethemil
kshake","added":"1302992738","status":"trusted","category":"207","imdb":""},{"id":"6987538","name":"Harry Potter and the
 Chamber of Secrets (2002) [eng subs]","info_hash":"F62847730234DE59DA0D6FC9C7F4E025EA5F4B57","leechers":"0","seeders":"
0","num_files":"3","size":"1756843845","username":"momamedienta","added":"1327611545","status":"trusted","category":"201
","imdb":"tt0295297"},{"id":"7714229","name":"Harry.Potter.And.The.Chamber.Of.Secrets.2002.BluRay.720p.H264","info_hash"
:"33E83B8E6D2E743179162234851452BEDE1F74A6","leechers":"1","seeders":"0","num_files":"7","size":"904108602","username":"
twentyforty","added":"1349880135","status":"trusted","category":"207","imdb":"tt0295297"},{"id":"10411774","name":"Harry
.Potter.and.the.Chamber.of.Secrets.2002.Extended.Cut.720p.B","info_hash":"92874DFD622663465403EC07088A6C213E6221CF","lee
chers":"1","seeders":"0","num_files":"4","size":"8590480567","username":"harks88","added":"1403541073","status":"trusted
","category":"207","imdb":"tt0295297"},{"id":"36669101","name":"Harry.Potter.And.The.Chamber.of.Secrets.2002.EXTENDED.10
80p.BR.x","info_hash":"3BABA971A9906AB55DF60D2D86A918BAAA08B87B","leechers":"9","seeders":"29","num_files":"4","size":"2
918879189","username":"Anonymous","added":"1602182693","status":"member","category":"207","imdb":"tt0295297"},{"id":"400
76006","name":"Harry.Potter.And.The.Chamber.Of.Secrets.2002.BDRip.2160p.UHD.HDR","info_hash":"3C501351C0DD44A423A949FD16
48C8D694AF54F0","leechers":"5","seeders":"8","num_files":"9","size":"30091781887","username":"gerald99","added":"1610279
815","status":"member","category":"211","imdb":"tt0295297"},{"id":"8918279","name":"Harry Potter and the Chamber of Secr
ets by J.K. Rowling.epub","info_hash":"DA96B971CCE7C33A5B6A79026B12ADC8093385F5","leechers":"0","seeders":"6","num_files
":"1","size":"1188665","username":"DethHolly","added":"1379188080","status":"member","category":"601","imdb":""},{"id":"
66057998","name":"Harry Potter and the Chamber of Secrets OPEN MATTE (2002) 1080p DTS-X 7.1 KK650","info_hash":"C732390A
EFE9CDBE1F6450B70DB028822D55D96B","leechers":"2","seeders":"6","num_files":"9","size":"21757711619","username":"the_magi
cman","added":"1676331993","status":"member","category":"207","imdb":""},{"id":"4694511","name":"[Gamecube]Harry Potter
and the Chamber of Secrets[NTSC].Robs","info_hash":"18E006C560FE7E22986B86EDEAA5116FFAC15520","leechers":"1","seeders":"
2","num_files":"2","size":"1459978287","username":"robs46","added":"1233507158","status":"member","category":"499","imdb
":""},{"id":"5039890","name":"Harry Potter and the Chamber of Secrets (2002)(ENG NL) 2Lions-Te","info_hash":"1D62B9475DA
EBA8C7A41E34D35EDA6180F157F43","leechers":"0","seeders":"2","num_files":"15","size":"4681188722","username":"Pioen","add
ed":"1249314235","status":"member","category":"202","imdb":"tt0295297"},{"id":"17973507","name":"Harry Potter and the Ch
amber of Secrets 2002 Hindi 720p","info_hash":"D02CE19C2D4C96B113302F25BDF18B257EB3E4BA","leechers":"1","seeders":"2","n
um_files":"1","size":"899853555","username":"2K3D","added":"1498091580","status":"member","category":"201","imdb":""},{"
id":"55438847","name":"Harry Potter and the Chamber of Secrets OPEN MATTE DTS:X KK650","info_hash":"15E539C8599306643D15
31791B9433A3C2526F83","leechers":"1","seeders":"2","num_files":"9","size":"21757711618","username":"the_magicman","added
":"1641082413","status":"member","category":"207","imdb":""},{"id":"3804563","name":"Harry Potter And The Chamber of Sec
rets [2002] DvdRip [Eng] - Th","info_hash":"52312A7F4EB0E9D758708B70974E0BEE224C2EA3","leechers":"0","seeders":"1","num_
files":"1","size":"943495168","username":"[Thizz]","added":"1189619268","status":"member","category":"201","imdb":"tt029
5297"},{"id":"4316964","name":"Harry Potter and the Chamber of Secrets_extended edition_x264man","info_hash":"6EDFBC5315
271D80FBF87FFBB631C2CCA511A83A","leechers":"1","seeders":"1","num_files":"5","size":"950001760","username":"x264man","ad
ded":"1217139623","status":"member","category":"201","imdb":""},{"id":"5084840","name":"Harry Potter And The Chamber Of
Secrets - Audio Book - Read By S","info_hash":"6C90ABC351F9E1B432F010D6399B6BC0923FA3A2","leechers":"0","seeders":"1","n
um_files":"18","size":"245020219","username":"Anonymous","added":"1252624238","status":"member","category":"102","imdb":
""},{"id":"5975983","name":"Harry Potter And The Chamber Of Secrets [PC-Game]","info_hash":"4993755112D03C259E1B186F1562
56C73E666AD8","leechers":"0","seeders":"1","num_files":"4","size":"644533719","username":"Igottaproblem","added":"129044
4655","status":"member","category":"401","imdb":""},{"id":"6541220","name":"Harry.Potter.and.the.Chamber.of.Secrets.2002
.[EN].[SE].[FI].DVDR","info_hash":"9848CDF9A9738B7BD03471F6DEE2785876F8FAE5","leechers":"0","seeders":"1","num_files":"2
","size":"4681437246","username":"Nidets","added":"1310690408","status":"member","category":"202","imdb":"tt0295297"},{"
id":"6925401","name":"Harry.Potter.and.the.Chamber.of.Secrets.2002.Extended.Cut.720p.B","info_hash":"FF836F850F5C71F46BD
FD97135001D7EA89822AB","leechers":"0","seeders":"1","num_files":"1","size":"6492582081","username":"S.C.L.R.","added":"1
325462122","status":"member","category":"207","imdb":""},{"id":"7497040","name":"Harry Potter And The Chamber Of Secrets
 Audiobook Jim Dale ameyk","info_hash":"CD79C899FA4C557684681326A5AC59600ED7BA46","leechers":"0","seeders":"1","num_file
s":"18","size":"130157250","username":"ameyk123","added":"1343899994","status":"member","category":"102","imdb":""},{"id
":"9965477","name":"Harry_Potter_and_the_Chamber_of_Secrets_2002_with_GREEK_SUBS","info_hash":"542D0B7D907FAC66F5B1C15FA
6075E76670D60B0","leechers":"0","seeders":"1","num_files":"3","size":"629087762","username":"warriorfun","added":"139755
5740","status":"member","category":"201","imdb":""},{"id":"12474056","name":"Harry Potter and the Chamber of Secrets [Au
dio Book Greek]","info_hash":"483D287B03B473FCE350E59F550C9EE14F4B66CC","leechers":"1","seeders":"1","num_files":"19","s
ize":"404882405","username":"greg5","added":"1442947258","status":"member","category":"102","imdb":""},{"id":"37659298",
"name":"Harry Potter and the Chamber of Secrets OPEN MATTE DTS-X KK650","info_hash":"0E98DE5FBD8E5FF628122D48EBFF636C28C
B5522","leechers":"0","seeders":"1","num_files":"7","size":"21754144719","username":"the_magicman","added":"1606536310",
"status":"member","category":"207","imdb":""},{"id":"41927158","name":"Harry Potter and the Chamber of Secrets OPEN MATT
E [1080p] KK650","info_hash":"1DB648320CAC9B0C6ECBD59D6BB7BC34EAFB9DD6","leechers":"0","seeders":"1","num_files":"5","si
ze":"4130759078","username":"the_magicman","added":"1615336277","status":"member","category":"207","imdb":""},{"id":"660
57997","name":"Harry Potter and the Chamber of Secrets OPEN MATTE (2002) [1080p] x264 KK650","info_hash":"68809F9CA356F9
958E88DAF77BBDE4CB413C6A49","leechers":"0","seeders":"1","num_files":"5","size":"4130759079","username":"the_magicman","
added":"1676331846","status":"member","category":"207","imdb":""},{"id":"3373955","name":"Harry Potter and the Chamber o
f Secrets DVDR","info_hash":"DC2099B7632176B3EC0D4BCD1A6ABDF7B7E17509","leechers":"1","seeders":"0","num_files":"1","siz
e":"4654104576","username":"zoit","added":"1124874980","status":"member","category":"202","imdb":"tt0295297"},{"id":"350
7099","name":"Harry Potter and The Chamber of Secrets - Read by Stephen Fry (C","info_hash":"CDA30C239C2ADF1679244A11BBE
49A33FB463854","leechers":"0","seeders":"0","num_files":"1","size":"91105468","username":"cjshim","added":"1153847358","
status":"member","category":"102","imdb":""},{"id":"3528358","name":"Harry.Potter.and.the.Chamber.of.Secrets.swesub(2002
) [ENG] [DVDr","info_hash":"335C58BFA7CC2C80B88C685E541A2E5027852B14","leechers":"1","seeders":"0","num_files":"3","size
":"1467939008","username":"viva la roos","added":"1159044281","status":"member","category":"201","imdb":""},{"id":"35681
07","name":"Harry Potter and the Chamber of Secrets","info_hash":"CC1723047E3E67846F39E26DF2D90BD48DCD1298","leechers":"
0","seeders":"0","num_files":"174","size":"520463013","username":"pshuart","added":"1164917496","status":"member","categ
ory":"102","imdb":""},{"id":"3630039","name":"HARRY POTTER AND THE CHAMBER OF SECRETS","info_hash":"31F45567E4C4E9A00376
6788917F6635A7CD409F","leechers":"1","seeders":"0","num_files":"6","size":"1389065243","username":"macsponge","added":"1
172812095","status":"member","category":"201","imdb":"tt0295297"},{"id":"3631884","name":"Harry Potter and the Chamber o
f Secrets [DivX]","info_hash":"5C33AF46C9B88CCB634E3647B960333942822430","leechers":"0","seeders":"0","num_files":"1","s
ize":"1667579894","username":"freebsdhost","added":"1173022827","status":"member","category":"201","imdb":"tt0295297"},{
"id":"3736166","name":"IPOD Harry Potter and the Chamber of Secrets","info_hash":"391C2421267CAA5148BEFEC166CA9DBFB8F36F
F9","leechers":"0","seeders":"0","num_files":"1","size":"758617845","username":"jwmtorrent","added":"1183917636","status
":"member","category":"201","imdb":"tt0295297"},{"id":"3739581","name":"Harry Potter and the Chamber of Secrets","info_h
ash":"88C93E525773B6328CE9ABD7145DCD4A2252DDA9","leechers":"1","seeders":"0","num_files":"16","size":"7469652063","usern
ame":"tirafondo","added":"1184238417","status":"member","category":"202","imdb":""},{"id":"3802357","name":"Harry Potter
 and The Chamber of Secrets","info_hash":"5293E27FBBC606BD3A7D855A66844C86BD09333B","leechers":"0","seeders":"0","num_fi
les":"700","size":"514464235","username":"3ogdy","added":"1189451861","status":"member","category":"401","imdb":""},{"id
":"3913499","name":"Harry.Potter.and.the.Chamber.of.Secrets.HD DVD.REMUX.1080p.VC-1.","info_hash":"AF27E83B587127C3A032A
1FD1C1BF95320AEED7E","leechers":"0","seeders":"0","num_files":"10","size":"22797438036","username":"far999","added":"119
6295475","status":"member","category":"207","imdb":""},{"id":"3929379","name":"Harry.Potter.And.The.Chamber.Of.Secrets[2
002]DvDrip[Eng]-Koffe","info_hash":"A8475895C38AD887F87B96D6D2BA9B17115EBA9D","leechers":"0","seeders":"0","num_files":"
2","size":"945141922","username":"-_-Koffe-_-","added":"1197353015","status":"member","category":"201","imdb":""},{"id":
"4176194","name":"Harry Potter and the Chamber of Secrets[2002]DvDrip[Eng]-FMUT","info_hash":"295298AA44BCC599156C05C461
584972C25149E9","leechers":"0","seeders":"0","num_files":"1","size":"943495168","username":"FalloutMU","added":"12101313
05","status":"member","category":"201","imdb":""}]

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
