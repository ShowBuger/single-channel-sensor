#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
from PyQt6.QtCore import Qt, QSize, QEvent, QTimer
from PyQt6.QtGui import QIcon, QPixmap
from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QPushButton, QLabel, QStackedWidget, QFrame,
    QSizePolicy, QApplication
)

# 导入自定义模块
from core.serial_manager import SerialManager
from core.data_logger import DataLogger
from core.app_settings import Settings
from gui.ui_functions import UIFunctions
from gui.widgets.title_bar import TitleBar
from gui.pages.serial_page import SerialPage
from gui.widgets.custom_grips import CustomGrip
from gui.themes.theme_manager import ThemeManager
# 导入样式
from gui.styles.style import Style


class MainWindow(QMainWindow):
    def __init__(self, initialize_immediately=True):
        super().__init__()

        # 设置窗口无框架
        self.setWindowFlag(Qt.WindowType.FramelessWindowHint)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        
        # 确保窗口可以接收键盘事件
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

        # 初始化窗口尺寸
        self.window_size = QSize(1000, 600)  # 默认尺寸
        self.setMinimumSize(self.window_size)

        # 导入设置
        self.settings = Settings()
        self.settings.load_settings()

        # 初始化主题管理器
        self.theme_manager = ThemeManager()
        # 从设置加载主题
        theme = self.settings.get_setting("theme")
        if theme:
            self.theme_manager.set_theme(theme)
        

        # 如果需要立即初始化
        if initialize_immediately:
            self.initialize_ui_components()
            self.initialize_data_manager()
            self.initialize_hardware_components()
            self.finalize_initialization()
        else:
            # 仅创建基本UI结构，不加载详细内容和组件
            self.create_basic_structure()

    def create_basic_structure(self):
        """创建基本UI结构，但不初始化详细内容"""
        # 创建主窗口部件
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)

        # 创建主布局
        self.main_layout = QVBoxLayout(self.central_widget)
        self.main_layout.setContentsMargins(0, 0, 0, 0)
        self.main_layout.setSpacing(0)

        # 创建内容容器
        self.content = QWidget()
        self.content_layout = QVBoxLayout(self.content)
        self.content_layout.setContentsMargins(0, 0, 0, 0)
        self.content_layout.setSpacing(0)
        
        # 标题栏（轻量级组件，可以立即创建）
        self.title_bar = TitleBar(self)
        self.content_layout.addWidget(self.title_bar)
        
        # 创建内容区域（空壳）
        self.content_area = QWidget()
        self.content_area_layout = QHBoxLayout(self.content_area)
        self.content_area_layout.setContentsMargins(0, 0, 0, 0)
        self.content_area_layout.setSpacing(0)
        self.content_layout.addWidget(self.content_area)
        
        # 添加内容到主布局
        self.main_layout.addWidget(self.content)
        
        # 添加自定义边框用于调整窗口大小
        self.left_grip = CustomGrip(self, Qt.Edge.LeftEdge)
        self.right_grip = CustomGrip(self, Qt.Edge.RightEdge)
        self.top_grip = CustomGrip(self, Qt.Edge.TopEdge)
        self.bottom_grip = CustomGrip(self, Qt.Edge.BottomEdge)

    def initialize_ui_components(self):
        """初始化UI相关组件"""
        # 如果已经创建了基本结构，跳过
        if hasattr(self, 'content_area'):
            # 创建右侧内容区域
            self.right_content = QWidget()
            self.right_content_layout = QVBoxLayout(self.right_content)
            self.right_content_layout.setContentsMargins(0, 0, 0, 0)
            self.right_content_layout.setSpacing(0)
            self.content_area_layout.addWidget(self.right_content)

            # 创建堆叠部件来管理页面
            self.pages = QStackedWidget()
            self.right_content_layout.addWidget(self.pages)
        else:
            # 如果基本结构尚未创建，则调用创建方法
            self.create_basic_structure()
            # 然后再次调用此方法完成UI初始化
            self.initialize_ui_components()

    def initialize_data_manager(self):
        """初始化数据管理相关组件"""
        # 初始化数据记录器
        self.data_logger = DataLogger(self.settings)

    def initialize_hardware_components(self):
        """初始化硬件控制相关组件"""
        # 初始化串口管理器
        self.serial_manager = SerialManager()

    def finalize_initialization(self):
        """完成初始化，创建剩余页面并连接信号"""
        # 创建串口通信页面
        self.serial_page = SerialPage(self, self.serial_manager)

        # 将页面添加到堆叠部件
        self.pages.addWidget(self.serial_page)

        # 连接信号槽
        self.connect_signals()

        # 应用样式表
        self.set_stylesheet()

        # 初始化页面，默认显示串口通信页面
        self.show_serial_page()

        # 处理自动连接串口
        self.handle_auto_connect()

    def connect_signals(self):
        """连接信号和槽"""
        # 连接标题栏按钮事件
        self.title_bar.minimize_btn.clicked.connect(self.showMinimized)
        self.title_bar.maximize_btn.clicked.connect(self.maximize_and_raise)
        self.title_bar.close_btn.clicked.connect(self.close)

    def set_stylesheet(self):
        """应用样式表"""
        style = Style.get_style(self.theme_manager.current_theme)
        self.setStyleSheet(style)

    def show_home_page(self):
        """显示主页（已删除，重定向到串口页）"""
        # 主页已删除，改为显示串口页
        self.show_serial_page()

    def show_serial_page(self):
        """显示串口页面"""
        self.pages.setCurrentWidget(self.serial_page)
        self.title_bar.title_label.setText("串口通信")
    


    def keyPressEvent(self, event):
        """键盘按下事件"""
        # 调用父类方法处理按键
        super().keyPressEvent(event)

    def mousePressEvent(self, event):
        """鼠标按下事件，用于移动窗口"""
        if event.button() == Qt.MouseButton.LeftButton:
            self.dragPos = event.globalPosition().toPoint()

    def resizeEvent(self, event):
        """窗口大小改变事件，用于调整边框大小"""
        # 更新边框位置
        UIFunctions.resize_grips(self)

    def closeEvent(self, event):
        """窗口关闭事件，用于保存设置和关闭串口"""
        # 关闭串口连接
        if self.serial_manager.is_connected():
            self.serial_manager.disconnect()

        # 保存设置
        if hasattr(self, "settings"):
            self.settings.save_settings()

        event.accept()

    def handle_auto_connect(self):
        """处理串口自动连接"""
        serial_settings = self.settings.get_setting("serial")
        
        # 检查是否启用了自动连接
        if serial_settings and serial_settings.get("auto_connect", False):
            last_port = serial_settings.get("last_port", "")
            
            # 如果有上次连接的端口信息
            if last_port:
                # 等待一小段时间让界面完全加载
                QTimer.singleShot(1000, lambda: self.try_auto_connect(last_port, serial_settings))
    
    def try_auto_connect(self, port, serial_settings):
        """尝试自动连接到上次的串口"""
        # 确保串口页面已初始化
        if hasattr(self, "serial_page"):
            # 获取可用端口列表
            ports = self.serial_manager.get_available_ports()

            # 填充端口列表
            self.serial_page.port_combo.clear()
            for p in ports:
                port_name = p.device
                port_description = f"{port_name} - {p.description}"
                self.serial_page.port_combo.addItem(port_description, port_name)

            # 在端口列表中查找目标端口
            port_found = False
            for index in range(self.serial_page.port_combo.count()):
                combo_text = self.serial_page.port_combo.itemText(index)
                combo_data = self.serial_page.port_combo.itemData(index)

                # 检查端口名称是否匹配（严格匹配，而不是包含关系）
                if combo_data == port or combo_text.startswith(f"{port} -"):
                    self.serial_page.port_combo.setCurrentIndex(index)
                    port_found = True
                    break

            # 如果找到了端口，尝试连接
            if port_found:
                # 设置波特率等参数
                baud_rate = str(serial_settings.get("baud_rate", 9600))
                if self.serial_page.baudrate_combo.findText(baud_rate) >= 0:
                    self.serial_page.baudrate_combo.setCurrentText(baud_rate)

                # 尝试连接
                self.serial_page.connect_to_port()
            else:
                print(f"上次使用的串口 {port} 未找到")

    def maximize_and_raise(self):
        """显示普通尺寸窗口并置顶"""
        # 首先显示窗口（如果尚未显示）
        if not self.isVisible():
            self.show()

        # 处理一下事件，确保窗口已经显示
        QApplication.processEvents()

        # 设置窗口为普通大小（使用默认尺寸1000x600）
        self.resize(self.window_size)

        # 设置窗口状态为正常且激活
        self.setWindowState(Qt.WindowState.WindowActive)

        # 确保窗口在前台并获得焦点
        self.raise_()
        self.activateWindow()

        # 处理一下事件，确保状态已经应用
        QApplication.processEvents()

        # 如果有最大化按钮，保持正常显示图标
        if hasattr(self, 'title_bar') and hasattr(self.title_bar, 'maximize_btn'):
            try:
                icons_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                      "resources", "images")
                maximize_icon_path = os.path.join(icons_path, "maximize.png")

                if os.path.exists(maximize_icon_path):
                    self.title_bar.maximize_btn.setIcon(QIcon(maximize_icon_path))

                self.title_bar.maximize_btn.setToolTip("最大化")
            except Exception as e:
                print(f"设置图标时发生错误: {e}")
                # 出错时继续运行，不中断应用程序