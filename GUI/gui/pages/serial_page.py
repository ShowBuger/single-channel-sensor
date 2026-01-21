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
    QGroupBox, QCheckBox, QFrame, QFileDialog, QMessageBox, QSizePolicy
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

        # 数据记录器
        if hasattr(main_window, 'data_logger'):
            self.data_logger = main_window.data_logger
        else:
            self.data_logger = None

        # 数据显示格式（固定为文本显示）
        self.display_hex = False
        # FLAG_RES: 使用每一位代表不同的使能标志
        # Bit 0: 标定使能
        # Bit 1: 死区过滤使能
        # Bit 2: 滑动窗口滤波使能
        # Bit 3: 预测补偿使能
        # Bit 4: 映射使能
        # Bit 5: 平滑过渡使能
        # 初始值: 0x3B = 0011 1011 (标定、死区、预测补偿、映射、平滑过渡 开启，滑动窗口滤波 关闭)
        self.flag_value = 0x3B

        # 创建UI
        self.setup_ui()

        # 连接信号
        self.connect_signals()

        # 初始化端口列表
        self.initialize_ports()

        # 初始连接状态指示
        self.set_connection_indicator(False)

    def setup_ui(self):
        """设置UI布局"""
        # 创建主布局
        self.main_layout = QVBoxLayout(self)
        self.main_layout.setContentsMargins(20, 20, 20, 20)
        self.main_layout.setSpacing(15)
        self.main_layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # 创建连接设置区域（仅保留端口号、波特率、刷新、连接）
        self.connection_group = QGroupBox("串口连接")
        self.connection_group.setObjectName("connectionGroup")
        self.connection_group.setMaximumWidth(900)
        self.connection_layout = QGridLayout(self.connection_group)
        self.connection_layout.setContentsMargins(5, 5, 5, 5)
        self.connection_layout.setHorizontalSpacing(8)
        self.connection_layout.setVerticalSpacing(5)

        row = 0
        # 串口选择（固定使用sensor0）
        port_label = QLabel("串口号")
        port_label.setStyleSheet("background-color: transparent;")
        self.connection_layout.addWidget(port_label, row, 0)
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(200)
        self.connection_layout.addWidget(self.port_combo, row, 1, 1, 2)

        row += 1
        # 波特率
        baudrate_label = QLabel("波特率")
        baudrate_label.setStyleSheet("background-color: transparent;")
        self.connection_layout.addWidget(baudrate_label, row, 0)
        self.baudrate_combo = QComboBox()
        self.baudrate_combo.addItems([
            "1200", "2400", "4800", "9600", "19200", "38400",
            "57600", "115200", "230400", "460800", "500000",
            "921600", "1000000"
        ])
        self.baudrate_combo.setCurrentText("115200")
        self.connection_layout.addWidget(self.baudrate_combo, row, 1)

        # 刷新按钮
        self.refresh_btn = QPushButton("刷新")
        self.refresh_btn.setObjectName("secondaryButton")
        self.refresh_btn.setMinimumHeight(28)
        self.refresh_btn.setMinimumWidth(60)
        self.connection_layout.addWidget(self.refresh_btn, row, 2)

        # 连接按钮
        self.connect_btn = QPushButton("连接")
        self.connect_btn.setObjectName("primaryButton")
        self.connect_btn.setMinimumHeight(28)
        self.connect_btn.setMinimumWidth(60)
        self.connection_layout.addWidget(self.connect_btn, row, 3)

        row += 1
        # 连接状态指示器
        status_layout = QHBoxLayout()
        self.connection_indicator = QLabel("●")
        self.connection_indicator.setObjectName("connectionIndicator")
        self.connection_indicator.setStyleSheet("color: gray; font-size: 14px;")
        self.connection_indicator.setFixedWidth(16)
        self.connection_status_text = QLabel("未连接")
        status_layout.addWidget(self.connection_indicator)
        status_layout.addWidget(self.connection_status_text)
        status_layout.addStretch()
        self.connection_layout.addLayout(status_layout, row, 0, 1, 4)

        self.main_layout.addWidget(self.connection_group)

        # 串口指令区域：SCAN/START/STOP + FLAG 使能按钮
        self.flag_group = QGroupBox("串口指令")
        self.flag_group.setObjectName("flagGroup")
        self.flag_group.setMaximumWidth(900)
        self.flag_layout = QVBoxLayout(self.flag_group)
        self.flag_layout.setContentsMargins(5, 5, 5, 5)
        self.flag_layout.setSpacing(5)

        # SCAN / START/STOP 行
        command_ctrl_layout = QHBoxLayout()
        command_ctrl_layout.addStretch()  # 左侧弹性空间

        self.scan_btn = QPushButton("SCAN")
        self.scan_btn.setObjectName("secondaryButton")
        self.scan_btn.setMinimumWidth(100)
        self.scan_btn.setMinimumHeight(35)
        self.scan_btn.setToolTip("扫描所有I2C通道，检测连接的传感器")
        command_ctrl_layout.addWidget(self.scan_btn)

        # 合并的 START/STOP 按钮
        self.start_stop_btn = QPushButton("START")
        self.start_stop_btn.setObjectName("primaryButton")
        self.start_stop_btn.setMinimumWidth(100)
        self.start_stop_btn.setMinimumHeight(35)
        self.start_stop_btn.setToolTip("启动所有检测到的传感器并进行自动标定")
        self.is_running = False  # 运行状态标志
        command_ctrl_layout.addWidget(self.start_stop_btn)

        command_ctrl_layout.addStretch()  # 右侧弹性空间
        self.flag_layout.addLayout(command_ctrl_layout)

        # GET / SET 行
        flag_ctrl_layout = QHBoxLayout()
        flag_ctrl_layout.addStretch()  # 左侧弹性空间

        self.flag_get_btn = QPushButton("GET")
        self.flag_get_btn.setObjectName("secondaryButton")
        self.flag_get_btn.setMinimumWidth(80)
        self.flag_get_btn.setMinimumHeight(30)
        flag_ctrl_layout.addWidget(self.flag_get_btn)

        self.flag_set_btn = QPushButton("SET")
        self.flag_set_btn.setObjectName("primaryButton")
        self.flag_set_btn.setMinimumWidth(80)
        self.flag_set_btn.setMinimumHeight(30)
        flag_ctrl_layout.addWidget(self.flag_set_btn)

        flag_ctrl_layout.addStretch()  # 右侧弹性空间
        self.flag_layout.addLayout(flag_ctrl_layout)

        # 标志按钮（可切换），与 FLAG_RES 各位一一对应
        self.flag_buttons = []
        flags_info = [
            ("标定", 0),          # Bit0: 标定使能
            ("死区过滤", 1),      # Bit1: 死区过滤使能
            ("滑动窗口", 2),      # Bit2: 滑动窗口滤波使能
            ("预测补偿", 3),      # Bit3: 预测补偿使能
            ("映射", 4),          # Bit4: 映射使能
            ("平滑过渡", 5),      # Bit5: 平滑过渡使能
        ]
        flags_layout = QHBoxLayout()
        flags_layout.setSpacing(8)
        flags_layout.addStretch()  # 左侧弹性空间
        for text, bit in flags_info:
            btn = QPushButton(text)
            btn.setCheckable(True)
            btn.setMinimumWidth(90)
            btn.setObjectName("flagButton")
            btn.clicked.connect(lambda checked, b=btn: self.update_flag_button_style(b, checked))
            self.flag_buttons.append((btn, bit))
            flags_layout.addWidget(btn)
        flags_layout.addStretch()  # 右侧弹性空间
        self.flag_layout.addLayout(flags_layout)

        # 初始化按钮显示
        self.apply_flag_value(self.flag_value)
        self.main_layout.addWidget(self.flag_group)

        # 创建发送区域（紧凑水平）
        self.send_frame = QFrame()
        self.send_frame.setObjectName("sendFrame")
        self.send_frame.setMaximumWidth(900)
        self.send_layout = QHBoxLayout(self.send_frame)
        self.send_layout.setContentsMargins(5, 3, 5, 3)
        self.send_layout.setSpacing(5)

        send_label = QLabel("发送:")
        send_label.setStyleSheet("background-color: transparent;")
        self.send_layout.addWidget(send_label)

        self.send_input = QTextEdit()
        self.send_input.setObjectName("sendInput")
        self.send_input.setPlaceholderText("输入要发送的数据")
        self.send_input.setMaximumHeight(35)
        self.send_input.setMinimumHeight(30)
        self.send_layout.addWidget(self.send_input)

        self.send_hex_check = QCheckBox("HEX")
        self.send_hex_check.setStyleSheet("background-color: transparent;")
        self.send_layout.addWidget(self.send_hex_check)

        self.send_btn = QPushButton("发送")
        self.send_btn.setObjectName("primaryButton")
        self.send_btn.setMaximumWidth(60)
        self.send_btn.setMinimumHeight(28)
        self.send_layout.addWidget(self.send_btn)

        self.main_layout.addWidget(self.send_frame)

        # 创建数据显示区域（充分利用空间）
        self.data_display_frame = QFrame()
        self.data_display_frame.setObjectName("dataDisplayFrame")
        self.data_display_layout = QVBoxLayout(self.data_display_frame)
        self.data_display_layout.setContentsMargins(0, 0, 0, 0)
        self.data_display_layout.setSpacing(3)

        # 接收区域
        self.receive_group = QGroupBox("数据监视")
        self.receive_group.setMaximumWidth(900)
        self.receive_layout = QVBoxLayout(self.receive_group)
        self.receive_layout.setContentsMargins(5, 5, 5, 5)
        self.receive_layout.setSpacing(5)

        # 接收设置栏（紧凑显示）
        self.receive_settings_layout = QHBoxLayout()
        self.receive_settings_layout.setContentsMargins(0, 0, 0, 0)
        self.receive_settings_layout.setSpacing(8)

        self.clear_receive_btn = QPushButton("清空")
        self.clear_receive_btn.setObjectName("secondaryButton")
        self.clear_receive_btn.setMaximumWidth(50)
        self.clear_receive_btn.setMinimumHeight(28)
        self.receive_settings_layout.addWidget(self.clear_receive_btn)

        self.receive_settings_layout.addStretch()

        self.receive_layout.addLayout(self.receive_settings_layout)

        # 接收文本区域 - 充分利用空间
        self.receive_text = QTextEdit()
        self.receive_text.setReadOnly(True)
        self.receive_text.setObjectName("receiveText")
        self.receive_text.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        # 设置背景色与应用背景一致
        self.receive_text.setStyleSheet("background-color: #282a36; color: #f8f8f2; border: 1px solid #44475a;")
        self.receive_layout.addWidget(self.receive_text)

        self.data_display_layout.addWidget(self.receive_group, 1)  # 给予充分的扩展空间

        # 添加数据显示区域到主布局（充分扩展）
        self.main_layout.addWidget(self.data_display_frame, 1)

        # 状态标签
        self.status_label = QLabel("未连接")
        self.status_label.setObjectName("statusLabel")
        self.main_layout.addWidget(self.status_label)

    def connect_signals(self):
        """连接信号和槽"""
        # 按钮事件
        self.refresh_btn.clicked.connect(self.initialize_ports)
        self.connect_btn.clicked.connect(self.toggle_connection)
        self.scan_btn.clicked.connect(self.send_scan_command)
        self.start_stop_btn.clicked.connect(self.toggle_start_stop)
        self.clear_receive_btn.clicked.connect(self.clear_receive)
        self.send_btn.clicked.connect(self.send_from_ui)
        self.flag_get_btn.clicked.connect(self.send_flag_get)
        self.flag_set_btn.clicked.connect(self.send_flag_set)

        # 串口管理器信号
        self.serial_manager.connected_signal.connect(self.on_connection_changed)
        self.serial_manager.error_signal.connect(self.on_error)
        self.serial_manager.received_data_signal.connect(self.on_data_received)

    def initialize_ports(self):
        """初始化串口列表"""
        # 清空列表
        self.port_combo.clear()

        # 获取可用端口
        ports = self.serial_manager.get_available_ports()

        # 添加到下拉框
        for port in ports:
            port_name = port.device
            port_description = f"{port_name} - {port.description}"
            self.port_combo.addItem(port_description, port_name)

    def send_scan_command(self):
        """发送SCAN命令"""
        if not self.serial_manager.is_connected():
            self.add_status_message("错误: 串口未连接", is_error=True)
            return
        self.serial_manager.send_data("ML SCAN\r\n")
        self.add_status_message("已发送: ML SCAN")

    def toggle_start_stop(self):
        """切换启动/停止状态"""
        if not self.serial_manager.is_connected():
            self.add_status_message("错误: 串口未连接", is_error=True)
            return

        if self.is_running:
            # 当前正在运行,发送停止命令
            self.serial_manager.send_data("ML STOP\r\n")
            self.add_status_message("已发送: ML STOP")

            # 更新按钮状态为START
            self.start_stop_btn.setText("START")
            self.start_stop_btn.setObjectName("primaryButton")
            self.start_stop_btn.setStyleSheet("")  # 清除自定义样式,使用主题样式
            self.start_stop_btn.setToolTip("启动所有检测到的传感器并进行自动标定")
            self.is_running = False
        else:
            # 当前已停止,发送启动命令
            self.serial_manager.send_data("ML START\r\n")
            self.add_status_message("已发送: ML START")

            # 更新按钮状态为STOP (红色)
            self.start_stop_btn.setText("STOP")
            self.start_stop_btn.setObjectName("stopButton")
            self.start_stop_btn.setStyleSheet("background-color: #ff5555; color: white; font-weight: bold;")
            self.start_stop_btn.setToolTip("停止所有运行中的传感器")
            self.is_running = True

    def send_flag_get(self):
        """发送获取FLAG命令"""
        if not self.serial_manager.is_connected():
            self.on_error("串口未连接，无法获取FLAG")
            return
        self.serial_manager.send_data("ML FLAG\r\n")
        self.add_status_message("已发送: ML FLAG")

    def send_flag_set(self):
        """根据按钮状态发送设置FLAG命令"""
        if not self.serial_manager.is_connected():
            self.on_error("串口未连接，无法设置FLAG")
            return
        flag_val = self.collect_flag_value()
        cmd = f"ML CHFLAG 0x{flag_val:02X}\r\n"
        self.serial_manager.send_data(cmd)
        self.add_status_message(f"已发送: {cmd.strip()}")

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

        # 获取参数：仅使用端口和波特率，其他使用默认值
        baud_rate = int(self.baudrate_combo.currentText())
        data_bits = 8
        parity = 'N'
        stop_bits = 1
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
            self.set_connection_indicator(True, port_info['port'])
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
            self.set_connection_indicator(False)
            self.add_status_message("串口已断开")

    def set_connection_indicator(self, connected, port_name=None):
        """更新连接指示器"""
        if connected:
            self.connection_indicator.setStyleSheet("color: #50fa7b; font-size: 14px;")
            text = f"已连接: {port_name}" if port_name else "已连接"
            self.connection_status_text.setText(text)
        else:
            self.connection_indicator.setStyleSheet("color: gray; font-size: 14px;")
            self.connection_status_text.setText("未连接")

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
        except UnicodeDecodeError:
            # 无法解码为UTF-8，显示为十六进制
            hex_str = binascii.hexlify(data).decode('ascii')
            formatted_hex = ' '.join(hex_str[i:i + 2] for i in range(0, len(hex_str), 2))
            data_str = f"[无法解码为文本] HEX: {formatted_hex.upper()}"

        # 更新接收区（文本显示）
        self.receive_text.append(data_str)

        # 解析 FLAG_RES 响应（格式: FLAG_RES: 0x1B）
        if data_str and "FLAG_RES" in data_str:
            try:
                parts = data_str.split(":")
                if len(parts) >= 2:
                    val_str = parts[1].strip()
                    # 去掉可能的前缀
                    if val_str.lower().startswith("0x"):
                        val_str = val_str[2:]
                    flag_val = int(val_str, 16)
                    self.flag_value = flag_val
                    self.apply_flag_value(flag_val)
                    self.add_status_message(f"FLAG_RES 接收: 0x{flag_val:02X}")
            except Exception:
                pass

        # 自动滚动到底部
        scrollbar = self.receive_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())


    def update_flag_button_style(self, btn, checked):
        """更新FLAG按钮的样式"""
        if checked:
            btn.setStyleSheet("background-color: #50fa7b; color: #282a36; font-weight: bold;")
        else:
            btn.setStyleSheet("")

    def apply_flag_value(self, flag_val):
        """根据flag值更新按钮状态"""
        for btn, bit in self.flag_buttons:
            checked = bool(flag_val & (1 << bit))
            btn.setChecked(checked)
            self.update_flag_button_style(btn, checked)

    def collect_flag_value(self):
        """根据按钮状态生成flag值"""
        val = 0
        for btn, bit in self.flag_buttons:
            if btn.isChecked():
                val |= (1 << bit)
        return val

    def clear_receive(self):
        """清空接收区"""
        self.receive_text.clear()


    def add_status_message(self, message, is_error=False):
        """添加状态信息到接收区"""
        color = "#FF5555" if is_error else "#55AA55"
        self.receive_text.append(f'<span style="color: {color};">>>> {message}</span>')

    def send_from_ui(self):
        """从UI发送数据"""
        if not self.serial_manager.is_connected():
            self.add_status_message("错误: 串口未连接", is_error=True)
            return
        
        # 获取输入数据
        raw_text = self.send_input.toPlainText()
        data = raw_text.strip()
        if not data:
            self.add_status_message("错误: 发送数据为空", is_error=True)
            return
        
        # 判断是否为HEX模式
        is_hex = self.send_hex_check.isChecked()
        
        try:
            # 文本模式下自动添加换行，避免命令粘连在一起
            send_payload = data
            if not is_hex:
                # 如果用户没有手动输入换行，则自动补充 \r\n
                if not send_payload.endswith("\n") and not send_payload.endswith("\r"):
                    send_payload = send_payload + "\r\n"

            # 调用串口管理器发送数据
            self.serial_manager.send_data(send_payload, is_hex=is_hex)
            self.add_status_message(f"已发送 ({len(send_payload)} 字节, 模式: {'HEX' if is_hex else 'TEXT'})")
            # 清空发送框
            self.send_input.clear()
        except Exception as e:
            self.add_status_message(f"发送错误: {str(e)}", is_error=True)
    