# -*- mode: python ; coding: utf-8 -*-

import os
import sys

# 获取当前目录的绝对路径
current_dir = os.path.dirname(os.path.abspath(SPEC))

# 定义数据文件
data_files = [
    # 包含整个resources目录
    (os.path.join(current_dir, 'resources'), 'resources'),
    # 包含config目录
    (os.path.join(current_dir, 'config'), 'config'),
]

# 定义图标文件（如果存在的话）
icon_file = os.path.join(current_dir, 'resources', 'images', 'icon.png')
if not os.path.exists(icon_file):
    icon_file = None

a = Analysis(
    ['main.py'],  # 主入口文件
    pathex=[current_dir],
    binaries=[],
    datas=data_files,
    hiddenimports=[
        'PyQt6.QtCore',
        'PyQt6.QtGui',
        'PyQt6.QtWidgets',
        'serial',
        'serial.tools.list_ports',
        'numpy',
        'scipy',
        'matplotlib',
        'pyqtgraph',
        'pyqtgraph.graphicsItems',
        'pyqtgraph.widgets',
        'pyqtgraph.parametertree',
        'pyqtgraph.parametertree.interactive',
        'pydoc',
        'inspect',
        'pprint',
        'doctest',
        'pkgutil',
        'importlib',
        'importlib.metadata',
        'csv',
        'json',
        're',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        'tkinter',
        'unittest',
        'pdb',
        'test',
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=None,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=None)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='单通道传感器系统',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,  # 不显示控制台窗口
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=icon_file,  # 设置应用程序图标
    version_file=None,
    distpath='dist_final',  # 使用不同的输出目录
)