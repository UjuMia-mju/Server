pushd %~dp0
python -m PyInstaller --onefile ^
  --name PacketGenerator ^
  --add-data "Templates;T emplates" ^
  --add-data "*.py;." ^
  --hidden-import ProtoParser ^
  --hidden-import google.protobuf ^
  --hidden-import google.protobuf.descriptor ^
  --hidden-import google.protobuf.message ^
  --collect-all google.protobuf ^
  PacketGenerator.py ^
  --debug=all
MOVE .\dist\PacketGenerator.exe .\GenPackets.exe
@RD /S /Q .\build
@RD /S /Q .\dist
DEL /S /F /Q .\PacketGenerator.spec
PAUSE