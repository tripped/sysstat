Template: sysstat/notice
Type: note
Description: WARNING: daily data files format has changed!
 Format of daily data statistics files has changed in version ${s_version}
 of sysstat and is *not* compatible with the previous one!
 .
 Existing data files need to be deleted.
Description-ru: ��������������: ������ ������ ���������� ������ ���������!
 ������ ������ ���������� ���������� ��������� � ������ ${s_version}
 sysstat � *��* ��������� � ����������� ��������!
 .
 ������������ ����� ������ ����� �������.

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
Description-ru: ������, ����� ����-������������ �������� ������ ��� ����� ������?
 ���� �� ������� '��' (�� ���������), �� ��� ������������ ����� ������
 � �������� /var/log/sysstat/ ����� �������.
 .
 ���� �� ������� '���', �� ��� ����� �� ����� �������, �� ����� ���������
 �������� sar, ��� ��� ��� �������� ������� �� �������.
