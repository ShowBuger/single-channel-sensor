#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import sys
import os
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QGuiApplication
from PyQt6.QtWidgets import QApplication
# 导入应用设置
from core.app_settings import Settings


# 确保可以找到资源文件
if getattr(sys, 'frozen', False):
    # 如果应用被打包
    APP_ROOT_PATH = os.path.dirname(sys.executable)
else:
    # 如果在开发环境中运行
    APP_ROOT_PATH = os.path.dirname(os.path.abspath(__file__))

# 将APP_ROOT_PATH添加到全局作用域，以便其他模块可以导入
import builtins
builtins.APP_ROOT_PATH = APP_ROOT_PATH

# 应用程序主入口
if __name__ == "__main__":
    # 创建应用程序实例
    app = QApplication(sys.argv)
    
    # 设置应用程序属性，确保QPainter正常工作
    # PyQt6 中属性名称可能与 PyQt5 不同
    try:
        # 尝试直接设置一些常见的高 DPI 相关属性
        common_attrs = [
            "AA_UseHighDpiPixmaps",
            "AA_EnableHighDpiScaling",
            "AA_DontCreateNativeWidgetSiblings"
        ]
        
        # 尝试直接设置这些常见属性
        for attr_name in common_attrs:
            try:
                if hasattr(Qt.ApplicationAttribute, attr_name):
                    attr = getattr(Qt.ApplicationAttribute, attr_name)
                    app.setAttribute(attr, True)
            except Exception as e:
                print(f"设置属性 {attr_name} 时出错: {e}")
        
        # 备份方法：动态查找其他高 DPI 相关属性
        try:
            # 使用反射获取所有应用程序属性
            aa_map = {name: getattr(Qt.ApplicationAttribute, name) 
                     for name in dir(Qt.ApplicationAttribute) 
                     if not name.startswith('_')}
            
            # 找出所有包含 HighDpi 字样的属性
            high_dpi_attrs = [attr for name, attr in aa_map.items() 
                             if ('HighDpi' in name or 'highdpi' in name.lower())
                             and name not in common_attrs]
            
            # 应用这些属性
            for attr in high_dpi_attrs:
                app.setAttribute(attr, True)
                print(f"动态启用属性: {attr}")
        except Exception as e:
            print(f"动态设置高 DPI 属性时出错: {e}")
    except Exception as e:
        print(f"设置应用程序属性时出错: {e}")
    
    # 在显示启动画面前先处理所有事件，确保Qt环境准备好
    app.processEvents()

    # 创建应用设置（轻量级，可以在主线程中初始化）
    settings = Settings()
    settings.load_settings()
    
    try:
        # 直接导入主窗口，确保APP_ROOT_PATH已设置
        from gui.main_window import MainWindow
        
        # 直接创建并初始化主窗口
        window = MainWindow(initialize_immediately=True)

        # 直接显示主窗口（普通尺寸并置顶）
        window.maximize_and_raise()
    
    except Exception as e:
        print(f"启动程序时出错: {e}")
        import traceback
        traceback.print_exc()
        # 出错时，尝试基本启动
        from gui.main_window import MainWindow
        window = MainWindow()
        window.show()
    
    # 执行应用程序
    sys.exit(app.exec())