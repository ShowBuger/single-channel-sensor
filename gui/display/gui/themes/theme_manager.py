#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import os
from PyQt6.QtCore import QObject, pyqtSignal


class ThemeManager(QObject):
    """主题管理器类，负责处理应用主题"""

    # 主题变更信号
    theme_changed = pyqtSignal(str)

    def __init__(self):
        super().__init__()

        # 默认主题
        self.default_theme = "dracula"

        # 当前主题
        self.current_theme = self.default_theme

        # 可用主题列表
        self.available_themes = {
            "dracula": {
                "name": "Dracula",
                "description": "暗色主题，紫罗兰色调",
                "primary_color": "#bd93f9",
                "secondary_color": "#6272a4",
                "background_color": "#282a36",
                "text_color": "#f8f8f2"
            }
        }

        # 加载保存的主题设置
        self.load_theme_settings()

    def get_theme_info(self, theme_name=None):
        """获取主题信息"""
        if theme_name is None:
            theme_name = self.current_theme

        if theme_name in self.available_themes:
            return self.available_themes[theme_name]
        else:
            return self.available_themes[self.default_theme]

    def get_available_themes(self):
        """获取所有可用主题"""
        return self.available_themes

    def set_theme(self, theme_name):
        """设置当前主题"""
        if theme_name in self.available_themes:
            if theme_name != self.current_theme:
                self.current_theme = theme_name
                self.save_theme_settings()
                self.theme_changed.emit(theme_name)
                return True
        return False

    def load_theme_settings(self):
        """加载主题设置"""
        try:
            settings_path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                         "config", "theme_settings.json")
            if os.path.exists(settings_path):
                with open(settings_path, 'r') as file:
                    settings = json.load(file)
                    if 'theme' in settings and settings['theme'] in self.available_themes:
                        self.current_theme = settings['theme']
        except Exception as e:
            print(f"加载主题设置错误: {e}")
            # 使用默认主题
            self.current_theme = self.default_theme

    def save_theme_settings(self):
        """保存主题设置"""
        try:
            # 确保配置目录存在
            config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                      "config")
            if not os.path.exists(config_dir):
                os.makedirs(config_dir)

            # 保存设置
            settings_path = os.path.join(config_dir, "theme_settings.json")
            with open(settings_path, 'w') as file:
                json.dump({'theme': self.current_theme}, file)
        except Exception as e:
            print(f"保存主题设置错误: {e}")