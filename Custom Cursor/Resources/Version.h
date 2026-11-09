#pragma once

// <Version>2.1.0</Version> in the .csproj was the last C# release; 3.0.0 is the first C++ one (a
// C# 2.x sees it as newer and updates itself to it through CustomCursorv3-0-0.zip)
#define APP_VERSION_MAJOR 3
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0
#define APP_VERSION_STRING "3.0.0"

// Exe properties (Explorer, Task Manager): <Authors>, <Product>, <Description>, <AssemblyName>
#define APP_PRODUCT     "CustomCursor"
#define APP_AUTHORS     "An Average Developer"
#define APP_DESCRIPTION "A tool for customizing Windows cursors"
#define APP_EXE_NAME    "CustomCursor.exe"   // <TargetName> in the .vcxproj
