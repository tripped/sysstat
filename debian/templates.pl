Template: sysstat/notice
Type: note
Description: WARNING: daily data files format has changed!
 Format of daily data statistics files has changed in version ${s_version}
 of sysstat and is *not* compatible with the previous one!
 .
 Existing data files need to be deleted.
Description-pl: UWAGA: zmieni� si� format plik�w z dziennymi statystykami!
 Format plik�w zawieraj�cych dzienne statystyki zosta� zmieniony w wersji
 ${s_version} programu sysstat i *nie* jest kompatybilny z poprzednimi
 wersjami programu!
 .
 Istniej�ce pliki z danymi powinny zosta� usuni�te.

Template: sysstat/remove_files
Type: boolean
Default: true
Description: Do you want post-installation script to remove these data files?
 If you say 'yes' (default), any existing data files in /var/log/sysstat/
 directory will be deleted.
 .
 If you say 'no', these files won't be deleted, but this will break the sar
 command, so you will have to delete them by hand.
Description-pl: Czy skrypt poinstalacyjny powinnien usun�� te pliki?
 Odpowied� twierdz�ca (domy�lna) na to pytanie spowoduje usuni�cie istniej�cych
 plik�w z dziennymi statystykami z katalogu /var/log/sysstat/.
 .
 Udzielenie odpowiedzi 'nie' spowoduje pozostawienie tych plik�w, przez co
 polecenie sar najprawdopodobniej przestanie dzia�a�, wi�c pliki te trzeba
 b�dzie usun�� r�cznie.
