param(
	[Parameter(Mandatory=$true, Position=0)]
	[string]$BuildDir,
	[Parameter(Mandatory=$true, Position=1)]
	[string]$RootDir,
	[Parameter(Position=2)]
	[string]$BuildConfig = "Release",
	[Parameter(Position=3)]
	[ValidateSet("auto", "x64", "arm64")]
	[string]$Arch = "auto"
)

function Get-CMakeCacheValue($cachePath, $name) {
	$match = Select-String -Path $cachePath -Pattern "^${name}:[^=]*=(.*)$" | Select-Object -First 1
	if ($match) {
		return $match.Matches[0].Groups[1].Value.Trim()
	}
	return $null
}

function Resolve-Arch($buildDir, $explicit) {
	if ($explicit -ne "auto") {
		return $explicit.ToLower()
	}
	$cachePath = Join-Path $buildDir "CMakeCache.txt"
	$platform = Get-CMakeCacheValue $cachePath "CMAKE_GENERATOR_PLATFORM"
	switch -Regex ($platform) {
		'^(?i)ARM64$' { return "arm64" }
		'^(?i)x64$'   { return "x64" }
	}

	# Ninja generators (the msvc-x64-*/msvc-arm64-* presets) never populate
	# CMAKE_GENERATOR_PLATFORM. Fall back to the MSVC host/target compiler
	# path, e.g. ".../bin/Hostx64/arm64/cl.exe".
	$compiler = Get-CMakeCacheValue $cachePath "CMAKE_CXX_COMPILER"
	switch -Regex ($compiler) {
		'[\\/]Host\w+[\\/]arm64[\\/]' { return "arm64" }
		'[\\/]Host\w+[\\/]x64[\\/]'   { return "x64" }
		default {
			throw "Cannot infer architecture from CMakeCache (CMAKE_GENERATOR_PLATFORM='$platform', CMAKE_CXX_COMPILER='$compiler'). Pass -Arch x64|arm64 explicitly."
		}
	}
}

function Get-WindowsStageDir($buildDir, $buildConfig) {
	$cachePath = Join-Path $buildDir "CMakeCache.txt"
	$stageRoot = Get-CMakeCacheValue $cachePath "SDRPP_WINDOWS_BUNDLE_STAGE_ROOT"
	if (-not $stageRoot) {
		$stageRoot = Join-Path $buildDir "stage\windows-bundle"
	}

	$stageDir = Join-Path $stageRoot $buildConfig
	if (-not (Test-Path $stageDir)) {
		throw "Windows bundle staging directory not found: $stageDir"
	}
	return $stageDir
}


$resolved_arch = Resolve-Arch $BuildDir $Arch
$package_dir = "sdriak_windows_$resolved_arch"
$zip_path = "$package_dir.zip"

& cmake --build $BuildDir --config $BuildConfig --target stage_windows_bundle
if ($LASTEXITCODE -ne 0) {
	throw "Failed to build stage_windows_bundle for config '$BuildConfig'."
}

$stage_dir = Get-WindowsStageDir $BuildDir $BuildConfig
$stage_leaf = Split-Path $stage_dir -Leaf
$renamed_dir = Join-Path (Split-Path $stage_dir -Parent) $package_dir

# A previous run that crashed between the renames below leaves the renamed
# directory behind; drop it so this run's rename can't collide with it.
if (Test-Path $renamed_dir) {
	Remove-Item -Recurse -Force $renamed_dir
}

Rename-Item -Path $stage_dir -NewName $package_dir
try {
	Compress-Archive -Path $renamed_dir -DestinationPath $zip_path -Force
}
finally {
	Rename-Item -Path $renamed_dir -NewName $stage_leaf
}
