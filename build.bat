@ECHO OFF
SET CONFIG=%1
SET TARGET=ALL_BUILD
SET NUM_THREADS=%NUMBER_OF_PROCESSORS%

IF NOT EXIST "%CONFIG%" MKDIR "%CONFIG%"
CD "%CONFIG%"
ECHO Building target '%TARGET%' in %CONFIG% configuration
cmake -DCMAKE_BUILD_TYPE=%CONFIG% ..
printf "Starting build using %s threads...\n" "%NUM_THREADS%"
cmake --build . --config "%CONFIG%" --target "%TARGET%" -j %NUM_THREADS%
cd ..
