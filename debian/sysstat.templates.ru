Template: sysstat/notice
Type: note
Description: WARNING: daily data files format has changed!
 Format of daily data statistics files has changed in version ${s_version}
 of sysstat and is *not* compatible with the previous one!
 .
 Existing data files need to be deleted.
Description-ru: Предупреждение: формат файлов ежедневных данных изменился!
 Формат файлов ежедневной статистики изменился в версии ${s_version}
 sysstat и *не* совместим с предыдущими версиями!
 .
 Существующие файлы данных нужно удалить.

Template: sysstat/remove_files
Type: boolean
Default: true
Description: Do you want post-installation script to remove these data files?
 If you say 'yes' (default), any existing data files in /var/log/sysstat/
 directory will be deleted.
 .
 If you say 'no', these files won't be deleted, but this will break the sar
 command, so you will have to delete them by hand.
Default-ru: true
Description-ru: Хотите, чтобы пост-установочный сценарий удалил эти файлы данных?
 Если вы скажете 'да' (по умолчанию), то все существующие файлы данных
 в каталоге /var/log/sysstat/ будут удалены.
 .
 Если вы скажете 'нет', то эти файлы не будут удалены, но будут запорчены
 командой sar, так что вам придется удалить их вручную.

