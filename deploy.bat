echo "Deploying..."
cd build
cmake --build . --config Release
cd ..
mkdir c:\tools\Zep
xcopy /s /Y build\Release\*.* c:\tools\Zep
  
  