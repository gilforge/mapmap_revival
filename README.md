MapMap Revival (Windows)
========================

> **Fork maintenu pour Windows 11** - Version portable pour ateliers pédagogiques

:warning: **Ce fork concerne uniquement Windows. Les versions Linux et macOS n'ont pas été corrigées et ne sont pas supportées.**

:robot: **Ce fork a été entièrement mis à jour par l'IA ![Claude](https://img.shields.io/badge/Claude-D97757?style=for-the-badge&logo=claude&logoColor=white) (Anthropic) sur l'initiative de @gilforge pour des travaux étudiants en arts graphiques.**

MapMap is a free video mapping software.

Projection mapping, also known as video mapping and spatial augmented
reality, is a projection technology used to turn objects, often
irregularly shaped, into a display surface for video projection.
These objects may be complex industrial landscapes, such as buildings.
By using specialized software, a two or three dimensional object is
spatially mapped on the virtual program which mimics the real
environment it is to be projected on. The software can interact with a
projector to fit any desired image onto the surface of that object.
This technique is used by artists and advertisers alike who can add
extra dimensions, optical illusions, and notions of movement onto
previously static objects. The video is commonly combined with, or
triggered by, audio to create an audio-visual narrative.


Version
-------
**0.7.0-windows** (January 2026)

This is a portable version - no installation required. Just extract and run `MapMap.exe`.


Changes from original MapMap 0.6.3
----------------------------------
- Fixed for Windows 11 with Qt 5.15.2 and GStreamer 1.26
- Fixed OpenGL rendering issues (Output Editor refresh)
- Fixed GStreamer auto-configuration (no manual PATH setup needed)
- Fixed shape creation when video dimensions are unavailable
- Replaced heavy QtWebEngine with lightweight QTextBrowser for shortcuts window
- Fixed QOSC library linking for Windows
- Portable version: works without installation


Requirements (Windows)
----------------------
- Windows 10/11 64-bit
- A graphics card with OpenGL support
- For multi-screen output: set Windows display mode to "Extend" (Win+P)


Build from source (Windows)
---------------------------
Prerequisites:
- Qt 5.15.2 MSVC 2019 64-bit
- Visual Studio 2019 Build Tools
- GStreamer 1.26 MSVC 64-bit (development package)

```powershell
# Build
powershell -ExecutionPolicy Bypass -File build.ps1

# Deploy Qt DLLs
powershell -ExecutionPolicy Bypass -File deploy.ps1

# Copy GStreamer DLLs manually from your GStreamer installation
```


Original Authors
----------------
* Sofian Audry: lead developer, user interface designer, project manager.
* Dame Diongue: developer.
* Alexandre Quessy: release manager, developer, technical writer, project manager.
* Mike Latona: user interface designer.
* Vasilis Liaskovitis: developer.


Contributors
------------
* Lucas Adair : developer, macOS packaging.
* Christian Ambaud: sponsor, inspiration.
* Alex Barry: user experience design.
* Eliza Bennett : documentation, chinese translation.
* Jonathan Roman Bland : developer.
* Sylvain Cormier: developer.
* Maxime Damecour: inspiration.
* Louis Desjardins: project manager.
* Ian Donnelly : user interface designer, documentation.
* Gene Felice : video package, documentation.
* Julien Keable: developer.
* Marc Lavallée: help with packaging.
* Matthew Loewens : documentation, developer.
* Madison Suniga : documentation.
* **Gilles Aubin** (@gilforge, gilles-aubin.net) : Windows 11 revival, portable version (2026).


Acknowledgements
----------------
The original MapMap project was made possible by the support of the International
Organization of La Francophonie (http://www.francophonie.org/).

:warning: **Note: This fork (mapmap_revival) is not affiliated with or supported by La Francophonie. The original project is no longer maintained.**

This Windows revival fork was created for educational purposes using AI-assisted development.


More info
---------
* Original project: http://mapmap.info
* Original repository: https://github.com/mapmapteam/mapmap


Licence
-------
[GNU GPL v3](https://github.com/mapmapteam/mapmap/blob/develop/LICENSE)
