#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from PyQt6.QtCore import Qt, pyqtSlot
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QPushButton, QLabel, QComboBox, QCheckBox,
    QGroupBox, QTabWidget, QSpinBox, QLineEdit,
    QFileDialog, QFrame, QSpacerItem, QSizePolicy, QMessageBox,
    QDoubleSpinBox
)
import os


class SettingsPage(QWidget):
    """设置页面，用于管理应用程序设置"""

    def __init__(self, main_window, theme_manager):
        super().__init__()

        self.main_window = main_window
        self.theme_manager = theme_manager

        # 设置UI
        self.setup_ui()

        # 连接信号
        self.connect_signals()

        # 加载当前设置
        self.load_current_settings()

    def setup_ui(self):
        """设置UI布局"""
        # 创建主布局
        self.main_layout = QVBoxLayout(self)
        self.main_layout.setContentsMargins(10, 10, 10, 10)
        self.main_layout.setSpacing(10)

        # 创建标题标签
        self.title_label = QLabel("设置")
        self.title_label.setObjectName("pageTitle")
        self.main_layout.addWidget(self.title_label)

        # 创建选项卡控件
        self.tab_widget = QTabWidget()
        self.tab_widget.setObjectName("settingsTabs")

        # 创建常规设置选项卡
        self.general_tab = QWidget()
        self.general_layout = QVBoxLayout(self.general_tab)
        self.general_layout.setContentsMargins(10, 10, 10, 10)
        self.general_layout.setSpacing(15)

        # 主题设置组
        self.theme_group = QGroupBox("主题设置")
        self.theme_layout = QGridLayout(self.theme_group)

        self.theme_layout.addWidget(QLabel("应用主题:"), 0, 0)
        self.theme_combo = QComboBox()
        self.theme_combo.addItems(["Dracula"])
        self.theme_layout.addWidget(self.theme_combo, 0, 1)

        self.apply_theme_btn = QPushButton("应用主题")
        self.apply_theme_btn.setObjectName("primaryButton")
        self.theme_layout.addWidget(self.apply_theme_btn, 0, 2)

        self.general_layout.addWidget(self.theme_group)

        # 显示设置组
        self.display_group = QGroupBox("显示设置")
        self.display_layout = QGridLayout(self.display_group)

        self.display_layout.addWidget(QLabel("文本字体:"), 0, 0)
        self.font_combo = QComboBox()
        self.font_combo.addItems(["Consolas", "Courier New", "DejaVu Sans Mono", "Monospace", "SimSun"])
        self.display_layout.addWidget(self.font_combo, 0, 1)

        self.display_layout.addWidget(QLabel("字体大小:"), 1, 0)
        self.font_size_spin = QSpinBox()
        self.font_size_spin.setRange(8, 20)
        self.font_size_spin.setValue(10)
        self.display_layout.addWidget(self.font_size_spin, 1, 1)

        self.display_layout.addWidget(QLabel("最大显示行数:"), 2, 0)
        self.max_lines_spin = QSpinBox()
        self.max_lines_spin.setRange(100, 10000)
        self.max_lines_spin.setValue(1000)
        self.max_lines_spin.setSingleStep(100)
        self.display_layout.addWidget(self.max_lines_spin, 2, 1)

        self.general_layout.addWidget(self.display_group)

        # 添加常规选项卡
        self.tab_widget.addTab(self.general_tab, "常规设置")

        # 创建串口设置选项卡
        self.serial_tab = QWidget()
        self.serial_layout = QVBoxLayout(self.serial_tab)
        self.serial_layout.setContentsMargins(10, 10, 10, 10)
        self.serial_layout.setSpacing(15)

        # 串口默认设置组
        self.serial_default_group = QGroupBox("串口默认设置")
        self.serial_default_layout = QGridLayout(self.serial_default_group)

        self.serial_default_layout.addWidget(QLabel("默认波特率:"), 0, 0)
        self.default_baud_combo = QComboBox()
        self.default_baud_combo.addItems(["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800", "500000", "921600", "1000000"])
        self.default_baud_combo.setCurrentText("9600")
        self.serial_default_layout.addWidget(self.default_baud_combo, 0, 1)

        self.serial_default_layout.addWidget(QLabel("默认数据位:"), 1, 0)
        self.default_data_bits_combo = QComboBox()
        self.default_data_bits_combo.addItems(["5", "6", "7", "8"])
        self.default_data_bits_combo.setCurrentText("8")
        self.serial_default_layout.addWidget(self.default_data_bits_combo, 1, 1)

        self.serial_default_layout.addWidget(QLabel("默认校验位:"), 2, 0)
        self.default_parity_combo = QComboBox()
        self.default_parity_combo.addItems(["无校验(N)", "奇校验(O)", "偶校验(E)", "标记校验(M)", "空白校验(S)"])
        self.serial_default_layout.addWidget(self.default_parity_combo, 2, 1)

        self.serial_default_layout.addWidget(QLabel("默认停止位:"), 3, 0)
        self.default_stop_bits_combo = QComboBox()
        self.default_stop_bits_combo.addItems(["1", "1.5", "2"])
        self.serial_default_layout.addWidget(self.default_stop_bits_combo, 3, 1)

        self.serial_layout.addWidget(self.serial_default_group)

        # 串口连接选项组
        self.serial_connection_group = QGroupBox("连接选项")
        self.serial_connection_layout = QVBoxLayout(self.serial_connection_group)

        self.auto_connect_check = QCheckBox("启动时自动连接到上次使用的串口")
        self.serial_connection_layout.addWidget(self.auto_connect_check)

        self.reconnect_check = QCheckBox("断开连接后自动尝试重连")
        self.serial_connection_layout.addWidget(self.reconnect_check)

        self.retry_layout = QHBoxLayout()
        self.retry_layout.addWidget(QLabel("重试次数:"))
        self.retry_spin = QSpinBox()
        self.retry_spin.setRange(1, 10)
        self.retry_spin.setValue(3)
        self.retry_layout.addWidget(self.retry_spin)
        self.retry_layout.addStretch()
        self.serial_connection_layout.addLayout(self.retry_layout)

        self.serial_layout.addWidget(self.serial_connection_group)

        # 添加串口选项卡
        self.tab_widget.addTab(self.serial_tab, "串口设置")

        # 创建数据记录选项卡
        self.logging_tab = QWidget()
        self.logging_layout = QVBoxLayout(self.logging_tab)
        self.logging_layout.setContentsMargins(10, 10, 10, 10)
        self.logging_layout.setSpacing(15)

        # 数据记录设置组
        self.logging_group = QGroupBox("数据记录设置")
        self.logging_layout_group = QVBoxLayout(self.logging_group)

        self.enable_logging_check = QCheckBox("启用数据记录")
        self.logging_layout_group.addWidget(self.enable_logging_check)

        # 数据记录路径设置
        self.log_path_layout = QHBoxLayout()
        self.log_path_layout.addWidget(QLabel("保存目录:"))
        self.log_path_edit = QLineEdit()
        self.log_path_edit.setReadOnly(True)
        self.log_path_layout.addWidget(self.log_path_edit)
        self.browse_log_path_btn = QPushButton("浏览...")
        self.browse_log_path_btn.setObjectName("secondaryButton")
        self.log_path_layout.addWidget(self.browse_log_path_btn)
        self.logging_layout_group.addLayout(self.log_path_layout)

        # 日志格式设置
        self.log_format_layout = QHBoxLayout()
        self.log_format_layout.addWidget(QLabel("日志格式:"))
        self.log_format_combo = QComboBox()
        self.log_format_combo.addItems(["TXT", "CSV"])
        self.log_format_layout.addWidget(self.log_format_combo)
        self.log_format_layout.addStretch()
        self.logging_layout_group.addLayout(self.log_format_layout)

        # 日志文件名设置
        self.auto_filename_check = QCheckBox("自动生成文件名 (日期时间)")
        self.auto_filename_check.setChecked(True)
        self.logging_layout_group.addWidget(self.auto_filename_check)

        self.include_timestamp_check = QCheckBox("数据中包含时间戳")
        self.include_timestamp_check.setChecked(True)
        self.logging_layout_group.addWidget(self.include_timestamp_check)

        self.logging_layout.addWidget(self.logging_group)

        # 添加数据记录选项卡
        self.tab_widget.addTab(self.logging_tab, "数据记录")

        # 创建传感器映射选项卡
        self.mapping_tab = QWidget()
        self.mapping_layout = QVBoxLayout(self.mapping_tab)
        self.mapping_layout.setContentsMargins(10, 10, 10, 10)
        self.mapping_layout.setSpacing(15)

        # 映射范围设置组
        self.mapping_group = QGroupBox("传感器数据映射范围")
        self.mapping_group_layout = QVBoxLayout(self.mapping_group)
        self.mapping_group_layout.setSpacing(10)

        # 说明文本
        mapping_info = QLabel("设置传感器原始数据到3D显示的映射范围\n数据将被映射到 -5 到 5 的显示范围内")
        mapping_info.setWordWrap(True)
        mapping_info.setStyleSheet("color: #8be9fd; padding: 5px;")
        self.mapping_group_layout.addWidget(mapping_info)

        # X轴映射范围
        x_mapping_layout = QHBoxLayout()
        x_label = QLabel("X轴范围:")
        x_label.setMinimumWidth(80)
        x_mapping_layout.addWidget(x_label)

        self.x_min_spinbox = QDoubleSpinBox()
        self.x_min_spinbox.setRange(-1000, 1000)
        self.x_min_spinbox.setValue(-10.0)
        self.x_min_spinbox.setDecimals(1)
        self.x_min_spinbox.setPrefix("最小: ")
        self.x_min_spinbox.setMinimumWidth(120)
        x_mapping_layout.addWidget(self.x_min_spinbox)

        self.x_max_spinbox = QDoubleSpinBox()
        self.x_max_spinbox.setRange(-1000, 1000)
        self.x_max_spinbox.setValue(10.0)
        self.x_max_spinbox.setDecimals(1)
        self.x_max_spinbox.setPrefix("最大: ")
        self.x_max_spinbox.setMinimumWidth(120)
        x_mapping_layout.addWidget(self.x_max_spinbox)
        x_mapping_layout.addStretch()
        self.mapping_group_layout.addLayout(x_mapping_layout)

        # Y轴映射范围
        y_mapping_layout = QHBoxLayout()
        y_label = QLabel("Y轴范围:")
        y_label.setMinimumWidth(80)
        y_mapping_layout.addWidget(y_label)

        self.y_min_spinbox = QDoubleSpinBox()
        self.y_min_spinbox.setRange(-1000, 1000)
        self.y_min_spinbox.setValue(-10.0)
        self.y_min_spinbox.setDecimals(1)
        self.y_min_spinbox.setPrefix("最小: ")
        self.y_min_spinbox.setMinimumWidth(120)
        y_mapping_layout.addWidget(self.y_min_spinbox)

        self.y_max_spinbox = QDoubleSpinBox()
        self.y_max_spinbox.setRange(-1000, 1000)
        self.y_max_spinbox.setValue(10.0)
        self.y_max_spinbox.setDecimals(1)
        self.y_max_spinbox.setPrefix("最大: ")
        self.y_max_spinbox.setMinimumWidth(120)
        y_mapping_layout.addWidget(self.y_max_spinbox)
        y_mapping_layout.addStretch()
        self.mapping_group_layout.addLayout(y_mapping_layout)

        # Z轴映射范围
        z_mapping_layout = QHBoxLayout()
        z_label = QLabel("Z轴范围:")
        z_label.setMinimumWidth(80)
        z_mapping_layout.addWidget(z_label)

        self.z_min_spinbox = QDoubleSpinBox()
        self.z_min_spinbox.setRange(-1000, 1000)
        self.z_min_spinbox.setValue(-10.0)
        self.z_min_spinbox.setDecimals(1)
        self.z_min_spinbox.setPrefix("最小: ")
        self.z_min_spinbox.setMinimumWidth(120)
        z_mapping_layout.addWidget(self.z_min_spinbox)

        self.z_max_spinbox = QDoubleSpinBox()
        self.z_max_spinbox.setRange(-1000, 1000)
        self.z_max_spinbox.setValue(10.0)
        self.z_max_spinbox.setDecimals(1)
        self.z_max_spinbox.setPrefix("最大: ")
        self.z_max_spinbox.setMinimumWidth(120)
        z_mapping_layout.addWidget(self.z_max_spinbox)
        z_mapping_layout.addStretch()
        self.mapping_group_layout.addLayout(z_mapping_layout)

        # 自动缩放说明
        auto_scale_info = QLabel("注意: 当数据超出设定范围时,将自动扩展映射范围")
        auto_scale_info.setWordWrap(True)
        auto_scale_info.setStyleSheet("color: #50fa7b; padding: 5px; font-size: 9pt;")
        self.mapping_group_layout.addWidget(auto_scale_info)

        self.mapping_layout.addWidget(self.mapping_group)
        self.mapping_layout.addStretch()

        # 添加传感器映射选项卡
        self.tab_widget.addTab(self.mapping_tab, "传感器映射")

        # 添加选项卡控件到主布局
        self.main_layout.addWidget(self.tab_widget)

        # 添加底部按钮区域
        self.button_layout = QHBoxLayout()
        self.button_layout.setContentsMargins(0, 10, 0, 0)
        self.button_layout.setSpacing(10)

        # 添加弹性空间
        self.button_layout.addStretch()

        # 重置按钮
        self.reset_btn = QPushButton("重置为默认")
        self.reset_btn.setObjectName("secondaryButton")
        self.button_layout.addWidget(self.reset_btn)

        # 保存按钮
        self.save_btn = QPushButton("保存设置")
        self.save_btn.setObjectName("primaryButton")
        self.button_layout.addWidget(self.save_btn)

        # 添加按钮布局到主布局
        self.main_layout.addLayout(self.button_layout)

    def connect_signals(self):
        """连接信号和槽"""
        # 应用主题按钮
        self.apply_theme_btn.clicked.connect(self.apply_theme)

        # 浏览日志目录按钮
        self.browse_log_path_btn.clicked.connect(self.browse_log_directory)

        # 保存设置按钮
        self.save_btn.clicked.connect(self.save_settings)

        # 重置按钮
        self.reset_btn.clicked.connect(self.reset_settings)

        # 主题选择下拉框
        self.theme_combo.currentIndexChanged.connect(self.on_theme_changed)

    def load_current_settings(self):
        """加载当前设置"""
        # 获取应用设置
        settings = self.main_window.settings
        
        # 加载主题设置
        current_theme = settings.get_setting("theme")
        if current_theme == "dracula":
            self.theme_combo.setCurrentText("Dracula")
        
        # 加载串口设置
        serial_settings = settings.get_setting("serial")
        if serial_settings:
            # 波特率
            baud_rate = str(serial_settings.get("baud_rate", 9600))
            if self.default_baud_combo.findText(baud_rate) >= 0:
                self.default_baud_combo.setCurrentText(baud_rate)
            
            # 数据位
            data_bits = str(serial_settings.get("data_bits", 8))
            if self.default_data_bits_combo.findText(data_bits) >= 0:
                self.default_data_bits_combo.setCurrentText(data_bits)
            
            # 自动连接选项
            self.auto_connect_check.setChecked(serial_settings.get("auto_connect", False))
            
            # 重试次数
            self.retry_spin.setValue(serial_settings.get("reconnect_attempts", 3))
        
        # 加载显示设置
        display_settings = settings.get_setting("display")
        if display_settings:
            # 字体
            font = display_settings.get("text_font", "Consolas")
            if self.font_combo.findText(font) >= 0:
                self.font_combo.setCurrentText(font)
            
            # 字体大小
            self.font_size_spin.setValue(display_settings.get("text_size", 10))
            
            # 最大行数
            self.max_lines_spin.setValue(display_settings.get("max_lines", 1000))
        
        # 加载日志设置
        logging_settings = settings.get_setting("logging")
        if logging_settings:
            # 是否启用日志
            self.enable_logging_check.setChecked(logging_settings.get("enabled", False))
            
            # 日志目录
            self.log_path_edit.setText(logging_settings.get("log_dir", ""))
            
            # 日志格式
            log_format = logging_settings.get("log_format", "txt").upper()
            if self.log_format_combo.findText(log_format) >= 0:
                self.log_format_combo.setCurrentText(log_format)
            
            # 自动文件名
            self.auto_filename_check.setChecked(logging_settings.get("auto_filename", True))
            
            # 包含时间戳
            self.include_timestamp_check.setChecked(logging_settings.get("include_timestamp", True))

        # 加载传感器映射设置
        mapping_settings = settings.get_setting("sensor_mapping")
        if mapping_settings:
            self.x_min_spinbox.setValue(mapping_settings.get("x_min", -10.0))
            self.x_max_spinbox.setValue(mapping_settings.get("x_max", 10.0))
            self.y_min_spinbox.setValue(mapping_settings.get("y_min", -10.0))
            self.y_max_spinbox.setValue(mapping_settings.get("y_max", 10.0))
            self.z_min_spinbox.setValue(mapping_settings.get("z_min", -10.0))
            self.z_max_spinbox.setValue(mapping_settings.get("z_max", 10.0))

    def apply_theme(self):
        """应用选中的主题"""
        theme_text = self.theme_combo.currentText()
        
        # 获取设置对象
        settings = self.main_window.settings
        
        if theme_text == "Dracula":
            # 更新主题管理器
            self.theme_manager.set_theme("dracula")
            # 同时更新设置
            settings.update_settings({"theme": "dracula"})
            
        # 应用样式表到主窗口
        self.main_window.set_stylesheet()

    def on_theme_changed(self, index):
        """主题选择改变时的响应"""
        # 自动应用主题
        self.apply_theme()

    def browse_log_directory(self):
        """浏览日志保存目录"""
        directory = QFileDialog.getExistingDirectory(
            self,
            "选择日志保存目录",
            self.log_path_edit.text() or os.path.join(APP_ROOT_PATH, "logs")
        )

        if directory:
            self.log_path_edit.setText(directory)
            
            # 立即应用目录设置，这样用户不需要点保存按钮就能改变日志目录
            if hasattr(self.main_window, "settings") and hasattr(self.main_window, "data_logger"):
                # 获取当前日志设置
                logging_settings = self.main_window.settings.get_setting("logging")
                if logging_settings is None:
                    logging_settings = {}
                else:
                    logging_settings = logging_settings.copy()
                
                # 更新日志目录
                logging_settings["log_dir"] = directory
                
                # 应用设置
                self.main_window.settings.update_settings({"logging": logging_settings})
                
                # 确保数据记录器更新目录
                self.main_window.data_logger.ensure_log_directory()

    def save_settings(self):
        """保存设置"""
        # 获取应用设置对象
        settings = self.main_window.settings
        
        # 保存主题设置
        theme_text = self.theme_combo.currentText()
        if theme_text == "Dracula":
            settings.update_settings({"theme": "dracula"})
        
        # 保存串口默认设置
        serial_settings = {
            "baud_rate": int(self.default_baud_combo.currentText()),
            "data_bits": int(self.default_data_bits_combo.currentText()),
            "parity": self.default_parity_combo.currentText()[0],
            "stop_bits": float(self.default_stop_bits_combo.currentText()),
            "auto_connect": self.auto_connect_check.isChecked(),
            "reconnect_attempts": self.retry_spin.value()
        }
        settings.update_settings({"serial": serial_settings})
        
        # 保存显示设置
        display_settings = {
            "text_font": self.font_combo.currentText(),
            "text_size": self.font_size_spin.value(),
            "max_lines": self.max_lines_spin.value()
        }
        settings.update_settings({"display": display_settings})
        
        # 获取当前日志设置状态
        old_logging_settings = settings.get_setting("logging")
        old_enabled = old_logging_settings.get("enabled", False) if old_logging_settings else False
        
        # 保存日志设置
        logging_settings = {
            "enabled": self.enable_logging_check.isChecked(),
            "log_dir": self.log_path_edit.text(),
            "log_format": self.log_format_combo.currentText().lower(),
            "auto_filename": self.auto_filename_check.isChecked(),
            "include_timestamp": self.include_timestamp_check.isChecked()
        }
        settings.update_settings({"logging": logging_settings})
        
        # 检查是否需要立即启动或停止数据记录
        if hasattr(self.main_window, "data_logger"):
            # 如果启用状态发生变化
            if old_enabled != logging_settings["enabled"]:
                if logging_settings["enabled"]:
                    self.main_window.data_logger.start_logging()
                    print("已启用数据记录")
                else:
                    self.main_window.data_logger.stop_logging()
                    print("已禁用数据记录")

        # 保存传感器映射设置
        mapping_settings = {
            "x_min": self.x_min_spinbox.value(),
            "x_max": self.x_max_spinbox.value(),
            "y_min": self.y_min_spinbox.value(),
            "y_max": self.y_max_spinbox.value(),
            "z_min": self.z_min_spinbox.value(),
            "z_max": self.z_max_spinbox.value()
        }
        settings.update_settings({"sensor_mapping": mapping_settings})

        # 如果传感器数据页面存在，更新其映射范围
        if hasattr(self.main_window, "sensor_data_page"):
            sensor_page = self.main_window.sensor_data_page
            sensor_page.base_mapping_range['x_min'] = mapping_settings['x_min']
            sensor_page.base_mapping_range['x_max'] = mapping_settings['x_max']
            sensor_page.base_mapping_range['y_min'] = mapping_settings['y_min']
            sensor_page.base_mapping_range['y_max'] = mapping_settings['y_max']
            sensor_page.base_mapping_range['z_min'] = mapping_settings['z_min']
            sensor_page.base_mapping_range['z_max'] = mapping_settings['z_max']
            # 同时重置当前映射范围
            sensor_page.mapping_range['x_min'] = mapping_settings['x_min']
            sensor_page.mapping_range['x_max'] = mapping_settings['x_max']
            sensor_page.mapping_range['y_min'] = mapping_settings['y_min']
            sensor_page.mapping_range['y_max'] = mapping_settings['y_max']
            sensor_page.mapping_range['z_min'] = mapping_settings['z_min']
            sensor_page.mapping_range['z_max'] = mapping_settings['z_max']

        # 通知用户保存成功
        QMessageBox.information(self, "保存成功", "设置已成功保存")

    def reset_settings(self):
        """重置为默认设置"""
        # 询问用户确认
        reply = QMessageBox.question(
            self, 
            "确认重置", 
            "您确定要将所有设置重置为默认值吗？这将丢失所有自定义设置。",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            # 重置应用设置
            self.main_window.settings.reset_to_defaults()
            
            # 重新加载设置到界面
            self.load_current_settings()
            
            # 通知用户
            QMessageBox.information(self, "重置完成", "所有设置已重置为默认值")