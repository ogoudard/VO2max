# vo2max.spec
# Run with: pyinstaller vo2max.spec --clean --noconfirm

import sys
from PyInstaller.utils.hooks import collect_data_files, collect_submodules

block_cipher = None
IS_WIN   = sys.platform == "win32"
IS_MAC   = sys.platform == "darwin"
IS_LINUX = sys.platform.startswith("linux")

SKIP_PREFIXES = (
    'pyqtgraph.jupyter',
    'OpenGL.raw.GLES2',
    'OpenGL.raw.GLES3',
    'OpenGL.GLES2',
    'OpenGL.GLES3',
    'OpenGL.Tk',
)

def filtered_submodules(pkg):
    return [
        m for m in collect_submodules(pkg)
        if not any(m.startswith(skip) for skip in SKIP_PREFIXES)
    ]

a = Analysis(
    ['vo2max.py'],
    pathex=[],
    binaries=[],
    datas=(
        collect_data_files('pyqtgraph') +
        collect_data_files('serial') +
        collect_data_files('OpenGL')
    ),
    hiddenimports=(
        filtered_submodules('pyqtgraph') +
        filtered_submodules('serial') +
        filtered_submodules('OpenGL') +
        [
            'serial.tools.list_ports',
            *(['serial.tools.list_ports_windows'] if IS_WIN   else []),
            *(['serial.tools.list_ports_osx']     if IS_MAC   else []),
            *(['serial.tools.list_ports_linux']   if IS_LINUX else []),
            'PyQt6.QtCore',
            'PyQt6.QtGui',
            'PyQt6.QtWidgets',
            'PyQt6.QtOpenGL',
            'PyQt6.QtOpenGLWidgets',
            'OpenGL',
            'OpenGL.GL',
            'OpenGL.arrays',
            'OpenGL.arrays.numpymodule',
            'numpy',
            'numpy.core._multiarray_umath',
            'numpy.core._multiarray_tests',
            'pkg_resources.py2_compat',
        ]
    ),
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        'tkinter', 'matplotlib', 'scipy', 'PIL', 'IPython',
        'jupyter_rfb', 'jupyter_client', 'ipykernel',
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,       # bundled directly into the exe
    a.zipfiles,
    a.datas,
    [],
    name='VO2Max',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    # icon='vo2max.ico',
)

# macOS only: single-file .app bundle
if IS_MAC:
    app = BUNDLE(
        exe,
        name='VO2Max.app',
        # icon='vo2max.icns',
        bundle_identifier='com.yourname.vo2max',
        info_plist={
            'NSHighResolutionCapable': True,
            'NSPrincipalClass': 'NSApplication',
        },
    )
