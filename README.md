WinAmp resumer plug-in
=======================

Saves state of Winamp every X seconds and resumes on startup.

This plug-in can be configured to save the status of the EQ, currently selected playlist song, and position in the currently playing song every X seconds. This information can then be restores when Winamp restarts.

(from http://www.winamp.com/plugin/resumer/27878 )

Based on general plug-in framework by Justin Frankel/Nullsoft.

Author:  Eddie Mansu

The source can freely be modified, reused & redistributed for non-
profitable uses. Use for commercial purposes prohibited.

Published here since 2013/12/04 by Rafael Vargas.

CHANGELOG
---------

v1.1:
  - this version has been modified so that it no longer resumes unless the song it's about to resume is the same one that was playing when it last saved information.

v1.1b: 
  - Fixed a buffer bug.

v1.2:
  - Fixed a bug and added a feature allowing songs to be resumed from the beginning instead of where they left off.

v1.3 (2013/12/04):
  - Support for newer windows and winamp versions (corrected the ini file path);
