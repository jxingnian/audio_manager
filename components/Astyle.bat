::-f �����в���صĴ���֮��������
::-p �ڲ��������߲���ո���=��+��-��
::-P ���������߲���ո���-dֻ�������������ո�-Dֻ���������
::-U �Ƴ��������߲���Ҫ�Ŀո�
::-N ��namespace������block��һ��tab������
::-n ��ʽ���󲻶�Դ�ļ�������
::-C ����public,pretected,private�ؼ��֣�һ��tab������
::-S switch��case�ؼ��֣�һ��tab������
::-K switch��case�ؼ��֣�������
::-w ��ʽ�����еĺ궨��
::-l ������ͺ����еĴ�����
::-a �����ű�������һ��
::-x ɾ��������У�3.x�汾��û�У�

@echo off
echo ��ǰ������ȫ·����%~f0
echo �밴�������������ʽ����·�������д���

pause
for /R ./ %%f in (*.c;*.h) do C:\AStyle\bin\AStyle.exe --style=kr --lineend=linux --pad-oper --unpad-paren --pad-header --convert-tabs --indent=spaces=4 --indent-labels --indent-preprocessor --align-pointer=name --align-reference=name --keep-one-line-blocks --keep-one-line-statements --attach-namespaces --preserve-date --max-instatement-indent=120 --suffix=none "%%f"
pause
