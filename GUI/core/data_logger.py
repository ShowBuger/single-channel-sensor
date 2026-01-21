#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import time
import datetime
import csv
from PyQt6.QtCore import QObject, pyqtSlot


class DataLogger(QObject):
    """数据记录器类，用于记录串口数据"""

    def __init__(self, settings):
        super().__init__()
        self.settings = settings
        self.log_file = None
        self.writer = None
        self.is_logging = False
        self.log_path = ""
        self.start_time = None
        
        # 创建日志目录
        self.ensure_log_directory()
        
        # 监听设置变更
        self.settings.settings_changed.connect(self.on_settings_changed)

    def ensure_log_directory(self):
        """确保日志目录存在"""
        logging_settings = self.settings.get_setting("logging")
        
        # 如果设置了自定义日志目录
        if logging_settings and logging_settings.get("log_dir"):
            log_dir = logging_settings["log_dir"]
        else:
            # 默认在项目目录下创建logs目录
            log_dir = os.path.join(APP_ROOT_PATH, "logs")
        
        # 确保目录存在
        if not os.path.exists(log_dir):
            try:
                os.makedirs(log_dir)
            except Exception as e:
                print(f"创建日志目录失败: {e}")
        
        self.log_dir = log_dir

    def start_logging(self):
        """开始记录数据"""
        if self.is_logging:
            return False
            
        logging_settings = self.settings.get_setting("logging")
        if not logging_settings or not logging_settings.get("enabled", False):
            return False
            
        try:
            # 获取日志格式
            log_format = logging_settings.get("log_format", "txt").lower()
            
            # 生成文件名
            if logging_settings.get("auto_filename", True):
                timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
                filename = f"serial_log_{timestamp}.{log_format}"
            else:
                filename = f"serial_log.{log_format}"
                
            # 完整路径
            self.log_path = os.path.join(self.log_dir, filename)
            
            # 打开文件
            if log_format == "csv":
                self.log_file = open(self.log_path, 'w', newline='', encoding='utf-8')
                self.writer = csv.writer(self.log_file)
                
                # 写入CSV表头
                if logging_settings.get("include_timestamp", True):
                    self.writer.writerow(["Timestamp", "Data"])
                else:
                    self.writer.writerow(["Data"])
            else:
                # 文本格式
                self.log_file = open(self.log_path, 'w', encoding='utf-8')
                
                # 写入文件头
                self.log_file.write(f"# 串口数据记录 - 开始时间: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                self.log_file.write(f"# {'='*50}\n\n")
            
            self.is_logging = True
            self.start_time = time.time()
            
            print(f"开始记录数据到: {self.log_path}")
            return True
            
        except Exception as e:
            print(f"开始记录数据失败: {e}")
            self.close_log_file()
            return False

    def stop_logging(self):
        """停止记录数据"""
        if not self.is_logging:
            return False
            
        self.close_log_file()
        self.is_logging = False
        
        print(f"数据记录已停止，文件: {self.log_path}")
        return True

    def close_log_file(self):
        """关闭日志文件"""
        if self.log_file:
            try:
                # 如果是文本文件，添加结束标记
                if not self.writer:
                    duration = time.time() - self.start_time if self.start_time else 0
                    self.log_file.write(f"\n\n# {'='*50}\n")
                    self.log_file.write(f"# 记录结束时间: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                    self.log_file.write(f"# 记录持续时间: {duration:.2f} 秒\n")
                
                self.log_file.close()
                self.log_file = None
                self.writer = None
            except Exception as e:
                print(f"关闭日志文件出错: {e}")

    @pyqtSlot(bytes)
    def log_data(self, data):
        """记录接收到的数据"""
        if not self.is_logging or not self.log_file:
            return
            
        try:
            # 尝试将数据解码为字符串
            try:
                data_str = data.decode('utf-8').strip()
            except UnicodeDecodeError:
                # 如果解码失败，显示十六进制
                data_str = data.hex(' ').upper()
                
            # 获取当前时间
            logging_settings = self.settings.get_setting("logging")
            if logging_settings and logging_settings.get("include_timestamp", True):
                timestamp = datetime.datetime.now().strftime("[%Y-%m-%d %H:%M:%S.%f")[:-3] + "]"
            else:
                timestamp = None
                
            # 写入数据
            if self.writer:  # CSV格式
                if timestamp:
                    self.writer.writerow([timestamp, data_str])
                else:
                    self.writer.writerow([data_str])
            else:  # 文本格式
                if timestamp:
                    self.log_file.write(f"{timestamp} {data_str}\n")
                else:
                    self.log_file.write(f"{data_str}\n")
                # 确保数据即时写入文件
                self.log_file.flush()
                
        except Exception as e:
            print(f"记录数据出错: {e}")

    @pyqtSlot(dict)
    def on_settings_changed(self, settings):
        """响应设置变更"""
        # 获取日志设置
        logging_settings = settings.get("logging", {})
        
        # 检查是否启用或禁用日志
        if logging_settings.get("enabled", False):
            if not self.is_logging:
                self.start_logging()
        else:
            if self.is_logging:
                self.stop_logging()
                
        # 检查是否更改了日志目录
        if self.is_logging and logging_settings.get("log_dir"):
            current_dir = os.path.dirname(self.log_path)
            if current_dir != logging_settings["log_dir"]:
                # 目录变更，重新开始记录
                self.stop_logging()
                self.ensure_log_directory()
                self.start_logging() 