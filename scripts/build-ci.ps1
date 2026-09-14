$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$work = Join-Path $env:RUNNER_TEMP 'source-deck-build'
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item $work -ItemType Directory | Out-Null

Write-Host '[1/4] Clone official OBS plugin template'
git clone --depth 1 https://github.com/obsproject/obs-plugintemplate.git "$work\template"
$tpl = "$work\template"

Write-Host '[2/4] Inject Source Deck source'
Remove-Item "$tpl\src\*" -Recurse -Force
Copy-Item "$root\src\*" "$tpl\src\" -Force
New-Item "$tpl\data\locale" -ItemType Directory -Force | Out-Null
Copy-Item "$root\data\locale\en-US.ini" "$tpl\data\locale\en-US.ini" -Force

@'
cmake_minimum_required(VERSION 3.28...3.30)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)
project(${_name} VERSION ${_version} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
include(compilerconfig)
include(defaults)
include(helpers)
add_library(${CMAKE_PROJECT_NAME} MODULE)
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  src/plugin-main.cpp
  src/source-deck-dock.cpp
  src/source-deck-dock.hpp
)
set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
'@ | Set-Content "$tpl\CMakeLists.txt" -Encoding UTF8

$buildspec = Get-Content "$tpl\buildspec.json" -Raw | ConvertFrom-Json
$buildspec.name = 'source-deck'
$buildspec.displayName = 'Source Deck'
$buildspec.version = '0.1.0'
$buildspec.author = 'Fruit Dragon'
$buildspec | ConvertTo-Json -Depth 20 | Set-Content "$tpl\buildspec.json" -Encoding UTF8

Write-Host '[3/4] Build x64 Release'
$env:CI = '1'
Push-Location $tpl
try {
  & pwsh .\.github\scripts\Build-Windows.ps1 -Target x64 -Configuration Release
} finally { Pop-Location }

Write-Host '[4/4] Stage installer payload'
$dll = Get-ChildItem "$tpl\release" -Recurse -Filter 'source-deck.dll' | Select-Object -First 1
if (-not $dll) { throw 'source-deck.dll was not produced.' }
$out = Join-Path $root 'dist'
$payload = Join-Path $out 'payload'
New-Item "$payload\obs-plugins\64bit" -ItemType Directory -Force | Out-Null
New-Item "$payload\data\obs-plugins\source-deck\locale" -ItemType Directory -Force | Out-Null
Copy-Item $dll.FullName "$payload\obs-plugins\64bit\source-deck.dll" -Force
Copy-Item "$root\data\locale\en-US.ini" "$payload\data\obs-plugins\source-deck\locale\en-US.ini" -Force
Write-Host "DLL staged: $($dll.FullName)"
