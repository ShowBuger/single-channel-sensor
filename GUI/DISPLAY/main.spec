# -*- mode: python ; coding: utf-8 -*-
import os

block_cipher = None

# 资源文件与 DLL
datas = [
    ('config', 'config'),
    ('resources', 'resources'),
    ('data', 'data'),
    ('exports', 'exports'),
]

# 尝试添加matplotlib的数据文件（字体等）
import matplotlib
try:
    mpl_data_dir = os.path.join(os.path.dirname(matplotlib.__file__), 'mpl-data')
    if os.path.exists(mpl_data_dir):
        datas.append((mpl_data_dir, 'matplotlib/mpl-data'))
except:
    pass

binaries = [
    ('libs/RM_Base.dll', 'libs'),
]

a = Analysis(
    ['main.py'],
    pathex=['.'],
    binaries=binaries,
    datas=datas,
    hiddenimports=[
        # PyQt6 相关隐藏导入
        'PyQt6',
        'PyQt6.QtCore',
        'PyQt6.QtGui',
        'PyQt6.QtWidgets',
        'PyQt6.sip',
        'PyQt6.QtOpenGL',
        'PyQt6.QtOpenGLWidgets',
        # 串口通信
        'serial',
        'serial.tools',
        'serial.tools.list_ports',
        # 科学计算
        'numpy',
        'numpy.core',
        'numpy.core._methods',
        'numpy.lib.format',
        'numpy.random',
        'numpy.linalg',
        # 小波变换
        'pywt',
        'pywt._extensions',
        'pywt._extensions._cwt',
        'pywt._extensions._dwt',
        'pywt._extensions._pywt',
        'pywt._extensions._swt',
        # Matplotlib 及其所有必需的后端
        'matplotlib',
        'matplotlib.pyplot',
        'matplotlib.backends',
        'matplotlib.backends.backend_agg',
        'matplotlib.backends.backend_qtagg',
        'matplotlib.backends.backend_qt5agg',
        'matplotlib.figure',
        'matplotlib.cm',
        'matplotlib.colors',
        'matplotlib.font_manager',
        'matplotlib.ticker',
        'matplotlib.dates',
        'matplotlib.units',
        'matplotlib.category',
        'matplotlib.patches',
        'matplotlib.lines',
        'matplotlib.path',
        'matplotlib.transforms',
        'matplotlib._layoutgrid',
        'matplotlib.tri',
        'matplotlib.tri.triangulation',
        'mpl_toolkits',
        'mpl_toolkits.mplot3d',
        'mpl_toolkits.mplot3d.art3d',
        'mpl_toolkits.mplot3d.proj3d',
        'mpl_toolkits.mplot3d.axes3d',
        # PyQtGraph
        'pyqtgraph',
        'pyqtgraph.graphicsItems',
        'pyqtgraph.exporters',
        'pyqtgraph.opengl',
        'pyqtgraph.Qt',
        # ctypes（用于DLL调用）
        'ctypes',
        'ctypes.wintypes',
        '_ctypes',
        # 其他标准库
        'csv',
        'json',
        'datetime',
        'logging',
        'logging.handlers',
        'threading',
        'queue',
        're',
        'tempfile',
        'traceback',
        'platform',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)

pyz = PYZ(
    a.pure,
    a.zipped_data,
    cipher=block_cipher,
)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='main',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,  # True = 显示控制台窗口（用于调试，看到错误信息）
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    # icon='resources/images/icon.png',  # 注释掉：Windows需要.ico格式，不是.png
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='main',
)