SET CONFIG=%1
SET TARGET=%2
SET NUM_THREADS=%3
IF "%NUM_THREADS%"=="%" SET NUM_THREADS=%NUMBER_OF_PROCESSORS%

IF NOT EXIST "%CONFIG%" MKDIR "%CONFIG%"
CD "%CONFIG%"
ECHO Building target '%TARGET%' in %CONFIG% configuration
IF "%CONFIG%"=="build" (
    ECHO Generating compilation commands...
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
    COPY ./compile_commands.json ../
) ELSE (
    cmake -DCMAKE_BUILD_TYPE=%CONFIG% ..
    printf "Starting build using %s threads...\n" "%NUM_THREADS%"
    cmake --build . --config "%CONFIG%" --target "%TARGET%" -j %NUM_THREADS%
)
