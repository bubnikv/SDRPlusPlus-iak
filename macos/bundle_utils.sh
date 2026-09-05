#==============================================#
#           MacOS Bundle Utilities             #
#         Copyright(C) Ryzerth, 2021           #
#==============================================#

# ========================= Customization =========================

# bundle_is_not_to_be_installed [dylib_name]
bundle_is_not_to_be_installed() {
    # NOTE: Customize this list to exclude libraries you don't want copied into the bundle
    if [ "$1" = "libsdriak_core.dylib" ]; then echo 1; fi
    if [ "$1" = "OpenGL" ]; then echo 1; fi
    if [ "$1" = "libc++.1.dylib" ]; then echo 1; fi
    if [ "$1" = "libSystem.B.dylib" ]; then echo 1; fi
    if [ "$1" = "Cocoa" ]; then echo 1; fi
    if [ "$1" = "IOKit" ]; then echo 1; fi
    if [ "$1" = "CoreFoundation" ]; then echo 1; fi
    if [ "$1" = "AppKit" ]; then echo 1; fi
    if [ "$1" = "CoreGraphics" ]; then echo 1; fi
    if [ "$1" = "CoreServices" ]; then echo 1; fi
    if [ "$1" = "Foundation" ]; then echo 1; fi
    if [ "$1" = "CoreAudio" ]; then echo 1; fi
    if [ "$1" = "AudioToolbox" ]; then echo 1; fi
    if [ "$1" = "AudioUnit" ]; then echo 1; fi
    if [ "$1" = "AVFoundation" ]; then echo 1; fi
    if [ "$1" = "libobjc.A.dylib" ]; then echo 1; fi
    if [ "$1" = "CFNetwork" ]; then echo 1; fi
    if [ "$1" = "SystemConfiguration" ]; then echo 1; fi
    if [ "$1" = "Security" ]; then echo 1; fi
    if [ "$1" = "AppleFSCompression" ]; then echo 1; fi
    if [ "$1" = "libsdrplay_api.so.3.14" ]; then echo 1; fi
    if [ "$1" = "libsdrplay_api.so.3.15" ]; then echo 1; fi
    if [ "$1" = "libxml2.2.dylib" ]; then echo 1; fi
}

# ========================= FOR INTERNAL USE ONLY =========================

# bundle_get_first_element [elem0] [elem1] [elem2] ...
bundle_get_first_element() {
    echo $1
}

# bundle_get_second_element [elem0] [elem1] [elem2] ...
bundle_get_second_element() {
    echo $2
}

# bundle_get_exec_deps [exec_path]
bundle_get_exec_deps() {
    DEPS_RAW=$(otool -L $1 | tail -n +2)

    # Iterate over all lines
    echo "$DEPS_RAW" | while read -r DEP; do
        echo $(bundle_get_first_element $DEP)
    done
}

# bundle_get_exec_rpaths [exec_path]
#
# Prints the binary's LC_RPATH entries, one per line, deduplicated.
#
# NOTE: `otool -l` dumps the load commands of *every* architecture of a
# universal binary, so a fat dylib (e.g. SDRplay's x86_64 + arm64
# libsdrplay_api.so.3) lists each of its rpaths once per slice. install_name_tool
# operates on the whole fat file at once, so a second `-delete_rpath` of the same
# path fails with "no LC_RPATH load command with path: ... found" and, under
# `set -e`, aborts the whole bundling step. Dedupe here so each rpath is deleted
# exactly once. Matching on the LC_RPATH command (rather than grepping for
# "path ") also avoids picking up unrelated load-command fields.
bundle_get_exec_rpaths() {
    otool -l "$1" | awk '
        $1 == "cmd" && $2 == "LC_RPATH" { in_rpath = 1; next }
        in_rpath && $1 == "path" { if (!seen[$2]++) print $2; in_rpath = 0 }
    '
}

# bundle_find_full_path [dep_path] [exec_rpaths]
bundle_find_full_path() {
    # Absolute (non-@rpath) reference: use it as-is.
    case "$1" in
        @rpath/*) ;;
        *) echo "$1"; return ;;
    esac

    local RPATH_NEXT=${1#@rpath/}

    # Search the binary's own rpaths. NOTE: this must NOT be a
    # `echo "$2" | while read` pipeline — the pipe runs the loop in a
    # subshell, so `return` there only exits the subshell and the function
    # falls through to the final `echo "$1"` below, emitting a second
    # (spurious) line. Callers capture the output unquoted, so that extra
    # line word-splits into an extra argument and breaks bundle_install_binary's
    # `[ $# -ne 3 ]` guard, silently dropping every @rpath dependency from the
    # bundle. Iterate with a plain `for` over newline-split rpaths instead.
    local OLDIFS=$IFS
    IFS='
'
    for RPATH in $2; do
        if [ -f "$RPATH/$RPATH_NEXT" ]; then
            IFS=$OLDIFS
            echo "$RPATH/$RPATH_NEXT"
            return
        fi
    done
    IFS=$OLDIFS

    # Search other common paths
    if [ -f "/usr/local/lib/$RPATH_NEXT" ]; then
        echo "/usr/local/lib/$RPATH_NEXT"
        return
    fi
    if [ -f "/Library/Frameworks/$RPATH_NEXT" ]; then
        echo "/Library/Frameworks/$RPATH_NEXT"
        return
    fi

    # Not found, give up
    echo "$1"
}

# ========================= Public Functions =========================

bundle_create_struct() {
    if [ $# -ne 1 ]; then
        echo "bundle_create_struct [bundle_path]";
        return
    fi
    
    mkdir -p $1
    mkdir -p $1/Contents
    mkdir -p $1/Contents/MacOS
    mkdir -p $1/Contents/Frameworks
    mkdir -p $1/Contents/Resources
    mkdir -p $1/Contents/Plugins
}

bundle_install_binary() {
    if [ $# -ne 3 ]; then
        echo "bundle_install_binary [bundle_path] [install_path] [exec_path]";
        return
    fi

    local EXEC_NAME=$(basename $3)
    local EXEC_DEST=$2/$EXEC_NAME

    # Check if file exists
    if [ ! -f $3 ]; then
        echo "==NOT== Installing" $3
        return
    fi

    # Get RPATHs
    local RPATHS=$(bundle_get_exec_rpaths $3)

    echo "Installing" $3

    # Copy it to its install location
    cp $3 $EXEC_DEST
    
    # Install dependencies and change path
    local DEPS=$(bundle_get_exec_deps $EXEC_DEST)
    echo "$DEPS" | while read -r DEP; do
        local DEP_NAME=$(basename $DEP)

        # Skip if this dep is blacklisted
        local NOT_TO_BE_INSTALLED=$(bundle_is_not_to_be_installed $DEP_NAME)
        if [ "$NOT_TO_BE_INSTALLED" = "1" ]; then
            continue
        fi

        # Skip if this dep is itself
        if [ "$DEP_NAME" = "$EXEC_NAME" ]; then
            continue
        fi

        local DEP_PATH=$(bundle_find_full_path "$DEP" "$RPATHS")

        # If the dependency is not installed, install it
        if [ ! -f "$1/Contents/Frameworks/$DEP_NAME" ]; then
            bundle_install_binary "$1" "$1/Contents/Frameworks" "$DEP_PATH"
        fi

        # Fix path
        install_name_tool -change $DEP @rpath/$DEP_NAME $EXEC_DEST
    done

    # Remove all its rpaths.
    #
    # Deliberately fatal (callers run under `set -e`). install_name_tool has no
    # -arch selector: it edits every slice of a universal binary or none, and if
    # one slice lacks the requested rpath it errors out without writing anything.
    # So a binary whose slices carry *different* rpaths cannot be normalized by
    # this function at all — it would need lipo -thin / edit / lipo -create.
    # Don't paper over that with a warning: a swallowed failure leaves a slice
    # holding a stale absolute rpath and, worse, makes the -add_rpath below fail
    # as a duplicate, silently shipping an .app that can't resolve its bundled
    # dylibs on one architecture. Fail loudly instead; install_name_tool's own
    # message names the path and the architecture.
    if [ "$RPATHS" != "" ]; then
        echo "$RPATHS" | while read -r RPATH; do
            install_name_tool -delete_rpath "$RPATH" "$EXEC_DEST"
        done
    fi

    # Add new single rpath. Safe unconditionally: every pre-existing rpath was
    # deleted above (or we aborted), so this cannot collide with a leftover.
    install_name_tool -add_rpath @loader_path/../Frameworks "$EXEC_DEST"
}

bundle_create_icns() {
    if [ $# -ne 2 ]; then
        echo "bundle_create_icns [image_path] [icns_file]";
        return
    fi

    mkdir $2.iconset
    sips -z 16 16     $1 --out $2.iconset/icon_16x16.png
    sips -z 32 32     $1 --out $2.iconset/icon_16x16@2x.png
    sips -z 32 32     $1 --out $2.iconset/icon_32x32.png
    sips -z 64 64     $1 --out $2.iconset/icon_32x32@2x.png
    sips -z 128 128   $1 --out $2.iconset/icon_128x128.png
    sips -z 256 256   $1 --out $2.iconset/icon_128x128@2x.png
    sips -z 256 256   $1 --out $2.iconset/icon_256x256.png
    sips -z 512 512   $1 --out $2.iconset/icon_256x256@2x.png
    sips -z 512 512   $1 --out $2.iconset/icon_512x512.png
    sips -z 1024 1024 $1 --out $2.iconset/icon_512x512@2x.png
    iconutil -c icns $2.iconset
    rm -R $2.iconset
}

bundle_create_plist() {
    if [ $# -ne 8 ]; then
        echo "bundle_create_plist [bundle_name] [display_name] [identifier] [version] [signature] [exec] [icon] [plist]";
        return
    fi
    
    echo '<?xml version="1.0" encoding="UTF-8"?>' >> $8
    echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' >> $8
    echo '' >> $8
    echo '<plist version="1.0">' >> $8
    echo '    <dict>' >> $8
    echo '        <key>CFBundleName</key><string>'$1'</string>' >> $8
    echo '        <key>CFBundleDisplayName</key><string>'$2'</string>' >> $8
    echo '        <key>CFBundleIdentifier</key><string>'$3'</string>' >> $8
    echo '        <key>CFBundleVersion</key><string>'$4'</string>' >> $8
    echo '        <key>CFBundlePackageType</key><string>APPL</string>' >> $8
    echo '        <key>CFBundleSignature</key><string>'$5'</string>' >> $8
    echo '        <key>CFBundleExecutable</key><string>'$6'</string>' >> $8
    echo '        <key>CFBundleIconFile</key><string>'$7'</string>' >> $8
    # macOS 10.14+ gates ALL audio input (including sound-card sources like the
    # QMX/QDX IQ stream) behind the microphone TCC permission. Without this
    # usage string macOS cannot present the consent prompt, so the input stream
    # silently delivers zeros (empty waterfall). Applies to any audio-input
    # capture in this bundle, not just QMX.
    echo '        <key>NSMicrophoneUsageDescription</key><string>SDRIAK needs microphone access to capture the IQ audio stream from sound-card SDR devices such as the QMX/QDX.</string>' >> $8
    echo '    </dict>' >> $8
    echo '</plist>' >> $8
}

bundle_sign() {
    if [ $# -lt 1 ]; then
        echo "bundle_sign [bundle_dir] [entitlements_plist]";
        return
    fi

    BUNDLE_DIR=$1
    ENTITLEMENTS=$2

    # When an entitlements file is supplied, sign with the hardened runtime so
    # the microphone (audio-input) entitlement is honored by TCC. Otherwise
    # fall back to the previous plain ad-hoc signature.
    if [ -n "$ENTITLEMENTS" ] && [ -f "$ENTITLEMENTS" ]; then
        codesign --force --deep --options runtime --entitlements "$ENTITLEMENTS" -s - "$BUNDLE_DIR"
    else
        codesign --force --deep -s - "$BUNDLE_DIR"
    fi
}
