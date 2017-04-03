# client-server
## **NAME** ##
**ftrestd** - serverova aplikace pro prenos souboru, komunikujici pomoci HTTP s pouzitim RESTful API.

**ftrest** - klientska aplikace  pro prenos souboru, komunikujici pomoci HTTP s pouzitim RESTful API.

## **SYNOPSIS** ##
**ftrestd** [**-r** *ROOT-FOLDER*] [**-p** *PORT*]

**ftrest** *COMMAND* *REMOTE-PATH* [*LOCAL-PATH*]

## **DESCRIPTION** ##
### ftrestd ###
Serverova aplikace nasloucha na zadanem portu (pokud neni zadan, tak na portu 6677), a ocekava</br>
HTTP pozadavek od uzivatele. Po ziskani pozadavku, aplikace zpracovava ho a na zaklade pozadovanem prikazu</br>
pokusi se splnit pozadavek, po cemz odesila klientovi HTTP odpoved o uspechu ci neuspechu (v zavislosti</br>
    na prikazu, s pozadovanymi datami). Serverova aplikace je schopna pracovat zaroven s nekolika uzivateli.</br>
### ftrest ###
Klientska aplikace zajistuje komunikace se serverovou aplikaci a odesila ji HTTP pozadavek s prikazem</br>
urcenym povinnym argumentem *COMMAND* podle rozhrani REST. Argumentem *REMOTE-PATH* se urcuje nad jakym</br>
adresarem ci souborem se bude provadet pozadovan operace. Po odeslani pozadavku na server, klientska</br>
aplikace ocekava HTTP odpoved od serveru (v zavislosti na prikazu, s pozadovanymi datami). Po ziskani</br>
odpovedi, klientska aplikace se unkonci uspesne, v pripade jakekoli chyby vypise na standartni chybovy</br>
vystup chybove hlaseni ziskane od serveru a ukonci se s prislusnym kodem.</br>
</br>

## **OPTIONS** ##
### ftrestd ###
**-r** *ROOT-FOLDER* - korenovy adresar, ve kterem serverova aplikace bude provadet pozadovane operace.

**-p** *PORT* - cislo portu, na kterem serverova aplikace bude naslouchat.
### ftrest ###
*COMMAND* - prikaz uzivatele.
+ **del** smaze soubor urceny REMOTE-PATH na serveru.
+ **get** zkopiruje soubor z REMOTE-PATH do aktualniho lokalniho adresare ci na misto urcene pomoci LOCAL-PATH je-li uvedeno.
+ **put** zkopiruje soubor z LOCAL-PATH do adresare REMOTE-PATH.
+ **lst**  vypise obsah vzdaleneho adresare na standardni vystup.
+ **mkd** vytvori adresar specifikovany v REMOTE-PATH na serveru.
+ **rmd** odstrani adresar specifikovany V REMOTE-PATH ze serveru.

*REMOTE-PATH* - cesta k souboru nebo adresari na serveru.

*LOCAL-PATH* - cesta v lokalnim souborovem systemu.


## **AUTHOR** ##
Ermak Aleksei
