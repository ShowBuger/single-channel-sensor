#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import binascii
import datetime
from PyQt6.QtCore import Qt, pyqtSlot, QTimer
from PyQt6.QtGui import QColor
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QPushButton, QLabel, QComboBox, QTextEdit,
    QGroupBox, QCheckBox, QFrame, QFileDialog, QMessageBox
)


class SerialPage(QWidget):
    """串口通信页面"""

    def __init__(self, main_window, serial_manager):
        super().__init__()

        self.main_window = main_window
        self.serial_manager = serial_manager

        # 如果主窗口有传感器数据管理器，获取它
        if hasattr(main_window, 'sensor_data_manager'):
            self.sensor_data_manager = main_window.sensor_data_manager
        else:
            self.sensor_data_manager = None

        # 数据显示格式
        self.display_hex = False

        # 创建UI
        self.setup_ui()

        # 连接信号
        self.connect_signals()

        # 刷新串口列表
        self.refresh_ports()

        # 创建定时器定期刷新串口列表
        self.port_refresh_timer = QTimer(self)
        self.port_refresh_timer.timeout.connect(self.refresh_ports)
        self.port_refresh_timer.start(2000)  # 每2秒刷新一次

    def setup_ui(self):
        """设置UI布局"""
        # 创建主布局
        self.main_layout = QVBoxLayout(self)
        self.main_layout.setContentsMargins(10, 10, 10, 10)
        self.main_layout.setSpacing(10)

        # 创建标题标签
        self.title_label = QLabel("串口通信")
        self.title_label.setObjectName("pageTitle")
        self.main_layout.addWidget(self.title_label)

        # 创建顶部控制区域
        self.control_frame = QFrame()
        self.control_frame.setObjectName("controlFrame")
        self.control_layout = QGridLayout(self.control_frame)
        self.control_layout.setContentsMargins(10, 10, 10, 10)
        self.control_layout.setSpacing(10)

        # 串口选择
        port_label = QLabel("串口:")
        port_label.setStyleSheet("background-color: transparent;")
        self.control_layout.addWidget(port_label, 0, 0)
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(150)
        self.control_layout.addWidget(self.port_combo, 0, 1)

        # 波特率
        baudrate_label = QLabel("波特率:")
        baudrate_label.setStyleSheet("background-color: transparent;")
        self.control_layout.addWidget(baudrate_label, 0, 2)
        self.baudrate_combo = QComboBox()
        self.baudrate_combo.addItems(["1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "460800", "500000", "921600", "1000000"])
        self.baudrate_combo.setCurrentText("115200")
        self.control_layout.addWidget(self.baudrate_combo, 0, 3)

        # 数据位
        databits_label = QLabel("数据位:")
        databits_label.setStyleSheet("background-color: transparent;")
        self.control_layout.addWidget(databits_label, 0, 4)
        self.databits_combo = QComboBox()
        self.databits_combo.addItems(["5", "6", "7", "8"])
        self.databits_combo.setCurrentText("8")
        self.control_layout.addWidget(self.databits_combo, 0, 5)

        # 停止位
        stopbits_label = QLabel("停止位:")
        stopbits_label.setStyleSheet("background-color: transparent;")
        self.control_layout.addWidget(stopbits_label, 1, 0)
        self.stopbits_combo = QComboBox()
        self.stopbits_combo.addItems(["1", "1.5", "2"])
        self.control_layout.addWidget(self.stopbits_combo, 1, 1)

        # 校验位
        parity_label = QLabel("校验位:")
        parity_label.setStyleSheet("background-color: transparent;")
        self.control_layout.addWidget(parity_label, 1, 2)
        self.parity_combo = QComboBox()
        self.parity_combo.addItems(["无校验(N)", "奇校验(O)", "偶校验(E)", "标记校验(M)", "空白校验(S)"])
        self.control_layout.addWidget(self.parity_combo, 1, 3)

        # 流控制
        flowctrl_label = QLabel("流控制:")
        flowctrl_label.setStyleSheet("background-color: transparent;")
        self.control_layout.addWidget(flowctrl_label, 1, 4)
        self.flowctrl_combo = QComboBox()
        self.flowctrl_combo.addItems(["无", "硬件", "软件"])
        self.control_layout.addWidget(self.flowctrl_combo, 1, 5)

        # 连接和刷新按钮
        self.btn_layout = QHBoxLayout()
        self.btn_layout.setSpacing(10)

        self.refresh_btn = QPushButton("刷新")
        self.refresh_btn.setObjectName("secondaryButton")
        self.refresh_btn.setMinimumWidth(80)
        self.btn_layout.addWidget(self.refresh_btn)

        self.connect_btn = QPushButton("连接")
        self.connect_btn.setObjectName("primaryButton")
        self.connect_btn.setMinimumWidth(80)
        self.btn_layout.addWidget(self.connect_btn)

        self.control_layout.addLayout(self.btn_layout, 1, 6)

        # 添加控制区域到主布局
        self.main_layout.addWidget(self.control_frame)

        # 创建数据显示区域
        self.data_display_frame = QFrame()
        self.data_display_frame.setObjectName("dataDisplayFrame")
        self.data_display_layout = QVBoxLayout(self.data_display_frame)
        self.data_display_layout.setContentsMargins(0, 0, 0, 0)
        self.data_display_layout.setSpacing(10)

        # 接收区域
        self.receive_group = QGroupBox("数据监视")
        self.receive_layout = QVBoxLayout(self.receive_group)

        # 接收设置栏
        self.receive_settings_layout = QHBoxLayout()

        self.clear_receive_btn = QPushButton("清空")
        self.clear_receive_btn.setObjectName("secondaryButton")
        self.receive_settings_layout.addWidget(self.clear_receive_btn)
        
        # 添加保存按钮
        self.save_receive_btn = QPushButton("保存数据")
        self.save_receive_btn.setObjectName("primaryButton")
        self.receive_settings_layout.addWidget(self.save_receive_btn)

        self.hex_display_check = QCheckBox("HEX显示")
        self.receive_settings_layout.addWidget(self.hex_display_check)

        self.auto_scroll_check = QCheckBox("自动滚动")
        self.auto_scroll_check.setChecked(True)
        self.receive_settings_layout.addWidget(self.auto_scroll_check)

        self.timestamp_check = QCheckBox("时间戳")
        self.receive_settings_layout.addWidget(self.timestamp_check)

        # 传感器数据处理选项
        self.parse_sensor_check = QCheckBox("传感器数据解析")
        self.parse_sensor_check.setChecked(True)
        self.receive_settings_layout.addWidget(self.parse_sensor_check)

        self.receive_settings_layout.addStretch()

        self.receive_layout.addLayout(self.receive_settings_layout)

        # 接收文本区域 - 修改样式确保底色一致
        self.receive_text = QTextEdit()
        self.receive_text.setReadOnly(True)
        self.receive_text.setObjectName("receiveText")
        self.receive_text.setMinimumHeight(300)  # 增加高度
        # 设置背景色与应用背景一致
        self.receive_text.setStyleSheet("background-color: #282a36; color: #f8f8f2; border: 1px solid #44475a;")
        self.receive_layout.addWidget(self.receive_text)

        self.data_display_layout.addWidget(self.receive_group)

        # 添加数据显示区域到主布局
        self.main_layout.addWidget(self.data_display_frame)

        # 状态标签
        self.status_label = QLabel("未连接")
        self.status_label.setObjectName("statusLabel")
        self.main_layout.addWidget(self.status_label)

    def connect_signals(self):
        """连接信号和槽"""
        # 按钮事件
        self.refresh_btn.clicked.connect(self.refresh_ports)
        self.connect_btn.clicked.connect(self.toggle_connection)
        self.clear_receive_btn.clicked.connect(self.clear_receive)
        self.save_receive_btn.clicked.connect(self.save_receive)

        # 复选框事件
        self.hex_display_check.stateChanged.connect(self.toggle_hex_display)

        # 串口管理器信号
        self.serial_manager.connected_signal.connect(self.on_connection_changed)
        self.serial_manager.error_signal.connect(self.on_error)
        self.serial_manager.received_data_signal.connect(self.on_data_received)

    def refresh_ports(self):
        """刷新可用串口列表"""
        current_port = self.port_combo.currentText()

        # 清空列表
        self.port_combo.clear()

        # 获取可用端口
        ports = self.serial_manager.get_available_ports()

        # 添加到下拉框
        for port in ports:
            port_name = port.device
            port_description = f"{port_name} - {port.description}"
            self.port_combo.addItem(port_description, port_name)

        # 恢复之前选中的端口
        if current_port:
            index = self.port_combo.findText(current_port, Qt.MatchFlag.MatchContains)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)

    def toggle_connection(self):
        """切换连接状态"""
        if self.serial_manager.is_connected():
            # 断开连接
            self.serial_manager.disconnect()
        else:
            # 连接
            self.connect_to_port()

    def connect_to_port(self):
        """连接到选定的串口"""
        if self.port_combo.count() == 0:
            self.on_error("没有可用的串口")
            return

        # 获取选中的端口
        port_data = self.port_combo.currentData()

        # 获取其他参数
        baud_rate = int(self.baudrate_combo.currentText())
        data_bits = int(self.databits_combo.currentText())

        # 解析停止位
        stop_bits_text = self.stopbits_combo.currentText()
        if stop_bits_text == "1":
            stop_bits = 1
        elif stop_bits_text == "1.5":
            stop_bits = 1.5
        else:
            stop_bits = 2

        # 解析校验位
        parity_text = self.parity_combo.currentText()
        if "奇校验" in parity_text:
            parity = 'O'
        elif "偶校验" in parity_text:
            parity = 'E'
        elif "标记校验" in parity_text:
            parity = 'M'
        elif "空白校验" in parity_text:
            parity = 'S'
        else:
            parity = 'N'

        # 解析流控制
        flow_control_text = self.flowctrl_combo.currentText()
        if flow_control_text == "硬件":
            flow_control = "hardware"
        elif flow_control_text == "软件":
            flow_control = "software"
        else:
            flow_control = None

        # 尝试连接
        success = self.serial_manager.connect(
            port=port_data,
            baud_rate=baud_rate,
            data_bits=data_bits,
            parity=parity,
            stop_bits=stop_bits,
            flow_control=flow_control
        )

        if not success:
            self.status_label.setText("连接失败")

    @pyqtSlot(bool)
    def on_connection_changed(self, connected):
        """连接状态变化时调用"""
        if connected:
            self.connect_btn.setText("断开")
            port_info = self.serial_manager.get_connection_info()
            self.status_label.setText(f"已连接到 {port_info['port']} - {port_info['baud_rate']}bps")
            self.add_status_message("串口连接成功")
            
            # 保存最后连接的串口信息到设置
            if hasattr(self.main_window, "settings"):
                # 获取当前串口设置
                serial_settings = self.main_window.settings.get_setting("serial")
                if serial_settings is None:
                    serial_settings = {}
                else:
                    # 创建副本以避免修改原始对象
                    serial_settings = serial_settings.copy()
                
                # 更新最后连接的端口信息
                serial_settings["last_port"] = port_info["port"]
                
                # 保存应用实际使用的端口参数作为默认值
                serial_settings["baud_rate"] = port_info["baud_rate"]
                serial_settings["data_bits"] = port_info["data_bits"]
                serial_settings["parity"] = port_info["parity"]
                serial_settings["stop_bits"] = port_info["stop_bits"]
                
                # 更新到设置
                self.main_window.settings.update_settings({"serial": serial_settings})
                # 立即保存设置
                self.main_window.settings.save_settings()
                
                self.add_status_message("已保存当前串口设置")
        else:
            self.connect_btn.setText("连接")
            self.status_label.setText("未连接")
            self.add_status_message("串口已断开")

    @pyqtSlot(str)
    def on_error(self, error_msg):
        """错误处理"""
        self.add_status_message(f"错误: {error_msg}", is_error=True)

    @pyqtSlot(bytes)
    def on_data_received(self, data):
        """接收到数据时调用"""
        # 尝试解码为字符串
        try:
            # 去除可能的结尾换行符
            data_str = data.decode('utf-8').strip()

            # 解析传感器数据
            if self.parse_sensor_check.isChecked() and self.sensor_data_manager:
                # 尝试解析为传感器数据
                if self.sensor_data_manager.parse_data(data_str):
                    # 解析成功，添加标记
                    if not data_str.startswith("[传感器数据]"):
                        data_str = f"[传感器数据] {data_str}"
        except UnicodeDecodeError:
            # 无法解码为UTF-8，显示为十六进制
            data_str = None

        # 处理接收到的数据
        if self.display_hex:
            # 十六进制显示
            hex_str = binascii.hexlify(data).decode('ascii')
            # 每两个字符添加一个空格
            formatted_hex = ' '.join(hex_str[i:i + 2] for i in range(0, len(hex_str), 2))
            display_data = formatted_hex.upper()
        else:
            # 文本显示
            if data_str is not None:
                display_data = data_str
            else:
                # 如果无法解码为UTF-8，则显示为十六进制
                hex_str = binascii.hexlify(data).decode('ascii')
                formatted_hex = ' '.join(hex_str[i:i + 2] for i in range(0, len(hex_str), 2))
                display_data = f"[无法解码为文本] HEX: {formatted_hex.upper()}"

        # 添加时间戳
        if self.timestamp_check.isChecked():
            timestamp = datetime.datetime.now().strftime("[%H:%M:%S.%f")[:-3] + "] "
            display_data = timestamp + display_data

        # 更新接收区
        self.receive_text.append(display_data)

        # 自动滚动
        if self.auto_scroll_check.isChecked():
            scrollbar = self.receive_text.verticalScrollBar()
            scrollbar.setValue(scrollbar.maximum())

    def toggle_hex_display(self, state):
        """切换十六进制显示模式"""
        self.display_hex = (state == Qt.CheckState.Checked.value)
        # 注意：这只会影响新接收的数据，不会改变已经接收的数据显示

    def clear_receive(self):
        """清空接收区"""
        self.receive_text.clear()

    def save_receive(self):
        """保存接收到的数据"""
        # 获取接收到的数据
        data = self.receive_text.toPlainText()
        if not data:
            self.add_status_message("没有数据可保存", is_error=True)
            return
            
        # 创建默认文件名
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        default_filename = f"received_data_{timestamp}.txt"

        # 使用当前目录
        default_dir = os.path.dirname(os.path.abspath(__file__))

        # 打开文件选择对话框
        filepath, _ = QFileDialog.getSaveFileName(
            self,
            "保存接收数据",
            os.path.join(default_dir, default_filename),
            "文本文件 (*.txt);;所有文件 (*.*)"
        )

        # 如果用户取消了对话框，filepath 为空
        if not filepath:
            return

        try:
            # 确保目录存在
            save_dir = os.path.dirname(filepath)
            if not os.path.exists(save_dir):
                os.makedirs(save_dir)

            # 保存数据到文件
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(data)

            self.add_status_message(f"数据已保存到: {filepath}")

        except Exception as e:
            self.add_status_message(f"保存数据错误: {str(e)}", is_error=True)

    def add_status_message(self, message, is_error=False):
        """添加状态信息到接收区"""
        color = "#FF5555" if is_error else "#55AA55"
        self.receive_text.append(f'<span style="color: {color};">>>> {message}</span>')

