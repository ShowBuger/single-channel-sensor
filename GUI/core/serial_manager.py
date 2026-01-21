#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from serial import Serial
import serial.tools.list_ports
import threading
import time
from PyQt6.QtCore import QObject, pyqtSignal, pyqtSlot


class SerialManager(QObject):
    """串口管理器类，负责处理串口连接和数据收发"""

    # 定义信号
    connected_signal = pyqtSignal(bool)  # 连接状态变化信号
    error_signal = pyqtSignal(str)  # 错误信号
    received_data_signal = pyqtSignal(bytes)  # 接收到数据信号

    def __init__(self):
        super().__init__()
        self.serial_port = None
        self.is_reading = False
        self.read_thread = None
        self.port_info = None

    def get_available_ports(self):
        """获取可用的串口列表"""
        ports = serial.tools.list_ports.comports()
        return list(ports)

    def connect(self, port, baud_rate=9600, data_bits=8, parity='N', stop_bits=1, timeout=1, flow_control=None):
        """连接到指定串口"""
        try:
            # 如果已连接，先断开
            if self.is_connected():
                self.disconnect()

            # 设置串口参数
            self.serial_port = Serial(
                port=port,
                baudrate=baud_rate,
                bytesize=data_bits,
                parity=parity,
                stopbits=stop_bits,
                timeout=timeout
            )

            # 配置流控制
            if flow_control:
                if flow_control == 'hardware':
                    self.serial_port.rtscts = True
                elif flow_control == 'software':
                    self.serial_port.xonxoff = True

            # 打开串口
            if not self.serial_port.is_open:
                self.serial_port.open()

            # 保存端口信息
            self.port_info = {
                'port': port,
                'baud_rate': baud_rate,
                'data_bits': data_bits,
                'parity': parity,
                'stop_bits': stop_bits,
                'flow_control': flow_control
            }

            # 启动读取线程
            self.is_reading = True
            self.read_thread = threading.Thread(target=self._read_data)
            self.read_thread.daemon = True
            self.read_thread.start()

            # 发射连接信号
            self.connected_signal.emit(True)
            return True

        except Exception as e:
            self.error_signal.emit(f"连接错误: {str(e)}")
            return False

    def disconnect(self):
        """断开串口连接"""
        if self.serial_port and self.serial_port.is_open:
            # 停止读取线程
            self.is_reading = False
            if self.read_thread and self.read_thread.is_alive():
                self.read_thread.join(timeout=1.0)

            # 关闭串口
            self.serial_port.close()
            self.serial_port = None
            self.port_info = None

            # 发射断开连接信号
            self.connected_signal.emit(False)
            return True
        return False

    def is_connected(self):
        """检查是否已连接"""
        return self.serial_port is not None and self.serial_port.is_open

    def get_connection_info(self):
        """获取当前连接信息"""
        return self.port_info

    def send_data(self, data, is_hex=False):
        """发送数据"""
        if not self.is_connected():
            self.error_signal.emit("串口未连接")
            return False

        try:
            # 如果是十六进制字符串，转换为字节
            if is_hex:
                # 移除所有空格
                data = data.replace(" ", "")
                # 转换为字节
                data = bytes.fromhex(data)
            elif isinstance(data, str):
                # 如果是字符串，转换为字节
                data = data.encode('utf-8')

            # 发送数据
            self.serial_port.write(data)
            return True

        except Exception as e:
            self.error_signal.emit(f"发送数据错误: {str(e)}")
            return False

    def _read_data(self):
        """读取数据线程函数"""
        buffer = b""  # 用于累积数据的缓冲区

        while self.is_reading and self.serial_port and self.serial_port.is_open:
            try:
                # 检查是否有数据可读
                if self.serial_port.in_waiting > 0:
                    # 读取数据并添加到缓冲区
                    data = self.serial_port.read(self.serial_port.in_waiting)
                    buffer += data

                    # 尝试从缓冲区中提取完整的行
                    if b'\n' in buffer:
                        lines = buffer.split(b'\n')
                        # 最后一个元素可能是不完整的行，保留在缓冲区
                        buffer = lines.pop()

                        # 发送每个完整的行
                        for line in lines:
                            if line.strip():  # 跳过空行
                                self.received_data_signal.emit(line + b'\n')

                # 短暂睡眠，减少CPU占用
                time.sleep(0.01)

            except Exception as e:
                self.error_signal.emit(f"读取数据错误: {str(e)}")
                # 如果发生错误，可能需要重新连接
                self.disconnect()
                break