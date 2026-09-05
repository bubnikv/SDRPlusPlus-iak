---
name: Bug report
about: Report crashes or unexpected behavior
title: ''
labels: bug
assignees: ''

---

# WARNING: Filling out the template below is NOT optional. Issues not filling out this template will be closed without review.

FIRST: Before reporting any bug, make sure that the bug you are reporting has not been reported before. Also, try the [latest SDRIAK build](https://github.com/bubnikv/sdriak/actions) if possible in case the bug has already been fixed.

**Hardware**
- CPU: 
- RAM:
- GPU: 
- SDR: (Remote or local? If remote, what protocol?)

**Software**
- Operating System: Name + Exact version (eg. Windows 10 x64, Ubuntu 22.04, MacOS 10.15)
- SDRIAK: Version + Build date (available either in the window title or in the credits menu which you can access by clicking on the SDRIAK icon in the top right corner of the software).

**Bug Description**
A clear description of the bug.

**Steps To Reproduce**
1. ...
2. ...
3. ...

**Only If SDRIAK fails to launch or the SDR fails to start:**
Run SDRIAK from a command line window with special parameters:
* On Windows, open a terminal and `cd` to SDRIAK's directory and run `.\sdriak.exe -c` (if running SDRIAK version 1.0.4 or older, use `-s` instead, though you should probably update SDRIAK instead...)
* On Linux: Open a terminal and run `sdriak -c`
* On macOS: Open a terminal and run `/path/to/SDRIAK.app/Contents/MacOS/sdriak -c`
Then, post the **entire** logs from start to after the issue. **DOT NOT truncate to where you *think* the error is...**

**Screenshots**
Add any screenshot that is relevant to the bug (GUI error messages, strange behavior, graphics glitch, etc...).

**Additional info**
Add any other relevant information.
