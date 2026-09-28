#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re
import time
import os
import csv
import json
import numpy as np
from PyQt6.QtCore import QObject, pyqtSignal, pyqtSlot
from datetime import datetime


class SensorData:
    """传感器数据类，用于存储单个传感器的数据"""

    def __init__(self, sensor_name, num_fields=3):
        self.sensor_name = sensor_name  # 传感器名称（字符串）
        self.num_fields = num_fields  # 字段数量
        self.timestamp = []
        self.data_fields = [[] for _ in range(num_fields)]  # 动态字段列表
        self.latest_values = [0] * num_fields  # 最新值
        self.last_active_time = time.time()  # 记录最后活跃时间

    # 兼容性属性：保持向后兼容
    @property
    def data1(self):
        """X轴数据（向后兼容）"""
        return self.data_fields[0] if self.num_fields >= 1 else []

    @property
    def data2(self):
        """Y轴数据（向后兼容）"""
        return self.data_fields[1] if self.num_fields >= 2 else []

    @property
    def data3(self):
        """Z轴数据（向后兼容）"""
        return self.data_fields[2] if self.num_fields >= 3 else []

    def add_data(self, *values):
        """添加一组数据，支持可变数量的字段"""
        # 首次添加时自动检测字段数量
        if len(self.timestamp) == 0 and len(values) != self.num_fields:
            self.num_fields = len(values)
            self.data_fields = [[] for _ in range(self.num_fields)]
            self.latest_values = [0] * self.num_fields

        # 验证字段数量一致性
        if len(values) != self.num_fields:
            raise ValueError(f"期望{self.num_fields}个字段，但收到{len(values)}个")

        # 添加时间戳
        self.timestamp.append(time.time())

        # 添加数据到对应字段
        for i, value in enumerate(values):
            self.data_fields[i].append(float(value))

        # 更新最新值
        self.latest_values = [float(v) for v in values]

        # 更新活跃时间
        self.last_active_time = time.time()

    def clear_data(self):
        """清空数据"""
        self.timestamp.clear()
        for field in self.data_fields:
            field.clear()
        self.latest_values = [0] * self.num_fields

    def get_latest_values(self):
        """获取最新的数据值"""
        return self.latest_values

    def get_data_count(self):
        """获取数据点数量"""
        return len(self.timestamp)

    def get_last_active_time(self):
        """获取最后活跃时间"""
        return self.last_active_time

    def is_active_within(self, seconds):
        """判断是否在指定秒数内活跃"""
        if seconds is None or seconds <= 0:
            return True
        return (time.time() - self.last_active_time) <= seconds

    def get_inactive_duration(self):
        """获取不活跃时长（秒）"""
        return time.time() - self.last_active_time


class SensorDataManager(QObject):
    """传感器数据管理器，负责解析和存储多个传感器的数据"""

    # 定义信号
    data_updated_signal = pyqtSignal(str, list)  # 数据更新信号 (传感器名称, [data1, data2, data3])
    data_parsed_signal = pyqtSignal(bool)  # 数据解析状态信号

    def __init__(self):
        super().__init__()

        # 传感器数据字典 {sensor_name: SensorData对象}
        self.sensors = {}

        # 数据正则表达式匹配模式：xxxx:data1,data2,...,dataN
        # 匹配任意字符串:一个或多个数值（用逗号分隔）
        self.pattern = re.compile(r'([^:]+):\s*([-+]?\d*\.?\d+(?:\s*,\s*[-+]?\d*\.?\d+)*)')

        # 数据文件目录
        self.data_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")
        if not os.path.exists(self.data_dir):
            os.makedirs(self.data_dir)

    def parse_data(self, data_str):
        """解析传感器数据字符串"""
        try:
            data_str = data_str.strip()
            # 查找匹配模式
            match = self.pattern.search(data_str)
            if match:
                # 提取传感器名称和数据字符串
                sensor_name = match.group(1).strip()
                values_str = match.group(2)

                # 分割并解析数值
                values = [float(v.strip()) for v in values_str.split(',')]

                # 至少需要1个字段
                if len(values) < 1:
                    self.data_parsed_signal.emit(False)
                    return False

                # 创建或获取传感器对象
                if sensor_name not in self.sensors:
                    self.sensors[sensor_name] = SensorData(sensor_name, num_fields=len(values))

                # 添加数据
                self.sensors[sensor_name].add_data(*values)

                # 发射数据更新信号
                self.data_updated_signal.emit(sensor_name, values)

                # 发射数据解析状态信号 - 成功
                self.data_parsed_signal.emit(True)

                return True
            else:
                # 发射数据解析状态信号 - 失败
                self.data_parsed_signal.emit(False)
                return False
        except Exception as e:
            print(f"数据解析错误: {e}")
            # 发射数据解析状态信号 - 失败
            self.data_parsed_signal.emit(False)
            return False

    def get_sensor_data(self, sensor_name):
        """获取指定传感器的数据对象"""
        if sensor_name in self.sensors:
            return self.sensors[sensor_name]
        return None

    def get_all_sensors(self, active_within_seconds=None):
        """获取所有传感器名称列表

        Args:
            active_within_seconds: 可选，仅返回N秒内活跃的传感器
                                  None表示返回所有传感器

        Returns:
            list: 传感器名称列表
        """
        if active_within_seconds is None:
            return list(self.sensors.keys())

        # 过滤活跃传感器
        active_sensors = []
        for name, sensor in self.sensors.items():
            if sensor.is_active_within(active_within_seconds):
                active_sensors.append(name)

        return active_sensors

    def clear_sensor_data(self, sensor_name=None):
        """清空指定传感器的数据，如果sensor_name为None则清空所有传感器数据"""
        if sensor_name is not None:
            if sensor_name in self.sensors:
                self.sensors[sensor_name].clear_data()
        else:
            for sensor in self.sensors.values():
                sensor.clear_data()

    def save_to_csv(self, sensor_name=None, filename=None, filepath=None):
        """将传感器数据保存为CSV文件

        Args:
            sensor_name: 要保存的传感器名称，如果为None则保存所有传感器
            filename: 文件名，如果为None则自动生成
            filepath: 完整文件路径，如果提供则直接使用该路径保存，忽略 filename 参数

        Returns:
            bool: 保存是否成功，返回实际保存的文件路径
        """
        try:
            if not filepath:
                # 如果没有指定文件名，自动生成
                if filename is None:
                    now = datetime.now()
                    timestamp = now.strftime("%Y%m%d_%H%M%S")
                    if sensor_name is not None:
                        # 清理传感器名称，移除不适合文件名的字符
                        safe_name = "".join(c for c in sensor_name if c.isalnum() or c in (' ', '-', '_')).rstrip()
                        filename = f"{safe_name}_data_{timestamp}.csv"
                    else:
                        filename = f"all_sensors_data_{timestamp}.csv"

                # 确保文件路径
                filepath = os.path.join(self.data_dir, filename)
            
            # 确保目录存在
            save_dir = os.path.dirname(filepath)
            if not os.path.exists(save_dir):
                os.makedirs(save_dir)

            # 写入CSV文件
            with open(filepath, 'w', newline='', encoding='utf-8-sig') as csvfile:
                writer = csv.writer(csvfile)

                # 写入表头
                if sensor_name is not None:
                    # 单个传感器
                    sensor_data = self.get_sensor_data(sensor_name)
                    if sensor_data:
                        # 动态生成表头
                        header = ['Timestamp'] + [f'Data{i+1}' for i in range(sensor_data.num_fields)]
                        writer.writerow(header)

                        # 写入数据
                        for i in range(sensor_data.get_data_count()):
                            # 将时间戳转换为可读格式
                            timestamp_readable = datetime.fromtimestamp(sensor_data.timestamp[i]).strftime(
                                "%Y-%m-%d %H:%M:%S.%f")[:-3]
                            row = [timestamp_readable] + [sensor_data.data_fields[j][i] for j in range(sensor_data.num_fields)]
                            writer.writerow(row)
                else:
                    # 所有传感器（处理不同字段数的情况）
                    sensor_names = self.get_all_sensors()
                    header = ['Timestamp']
                    for sname in sensor_names:
                        sensor_data = self.get_sensor_data(sname)
                        if sensor_data:
                            header.extend([f'{sname}_Data{i+1}' for i in range(sensor_data.num_fields)])
                    writer.writerow(header)

                    # 找出最大数据点数
                    max_data_points = 0
                    for sname in sensor_names:
                        sensor_data = self.get_sensor_data(sname)
                        if sensor_data:
                            max_data_points = max(max_data_points, sensor_data.get_data_count())

                    # 写入数据行（对齐不同字段数）
                    for i in range(max_data_points):
                        row = []
                        # 使用第一个传感器的时间戳
                        first_sensor = self.get_sensor_data(sensor_names[0])
                        if first_sensor and i < first_sensor.get_data_count():
                            # 将时间戳转换为可读格式
                            timestamp_readable = datetime.fromtimestamp(first_sensor.timestamp[i]).strftime(
                                "%Y-%m-%d %H:%M:%S.%f")[:-3]
                            row.append(timestamp_readable)
                        else:
                            row.append('')

                        # 添加所有传感器的数据
                        for sname in sensor_names:
                            sensor_data = self.get_sensor_data(sname)
                            if sensor_data and i < sensor_data.get_data_count():
                                row.extend([sensor_data.data_fields[j][i] for j in range(sensor_data.num_fields)])
                            else:
                                row.extend([''] * sensor_data.num_fields)
                        writer.writerow(row)

            return True, filepath
        except Exception as e:
            print(f"保存CSV文件错误: {e}")
            return False, None

    def get_sensor_statistics(self, sensor_name):
        """获取传感器数据的统计信息"""
        sensor_data = self.get_sensor_data(sensor_name)
        if not sensor_data or sensor_data.get_data_count() == 0:
            return None

        # 动态生成统计信息
        stats = {}
        for i in range(sensor_data.num_fields):
            field_name = f'data{i+1}'
            stats[field_name] = {
                'min': min(sensor_data.data_fields[i]),
                'max': max(sensor_data.data_fields[i]),
                'mean': np.mean(sensor_data.data_fields[i]),
                'std': np.std(sensor_data.data_fields[i])
            }

        return stats