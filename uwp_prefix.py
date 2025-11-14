#!/usr/bin/env python3

"""
UWP Prefix Configuration
Sets up Wine prefix for UWP application support
"""

import os
import subprocess
import logging
from pathlib import Path
from typing import Optional


class UWPPrefixSetup:
    """Configure Wine prefix for UWP applications"""

    # WinRT DLLs that should be set to builtin
    WINRT_DLLS = [
        'combase',
        'api-ms-win-core-winrt-l1-1-0',
        'api-ms-win-core-winrt-error-l1-1-0',
        'api-ms-win-core-winrt-error-l1-1-1',
        'api-ms-win-core-winrt-string-l1-1-0',
        'api-ms-win-core-winrt-registration-l1-1-0',
        'api-ms-win-core-winrt-roparameterizediid-l1-1-0',
        'api-ms-win-core-winrt-errorprivate-l1-1-1',
    ]

    # Additional DLLs that may be needed for UWP
    UWP_SUPPORT_DLLS = [
        'kernel32',
        'kernelbase',
        'ntdll',
        'user32',
        'gdi32',
        'advapi32',
        'ole32',
        'oleaut32',
        'rpcrt4',
        'bcrypt',
    ]

    def __init__(self, prefix_path: Path, dist_path: Path, log_level=logging.INFO):
        """
        Initialize UWP prefix setup

        Args:
            prefix_path: Path to Wine prefix
            dist_path: Path to Proton dist directory
            log_level: Logging level
        """
        self.prefix_path = Path(prefix_path)
        self.dist_path = Path(dist_path)

        self.logger = logging.getLogger('UWPPrefix')
        self.logger.setLevel(log_level)

        if not self.logger.handlers:
            handler = logging.StreamHandler()
            formatter = logging.Formatter('[%(levelname)s] %(message)s')
            handler.setFormatter(formatter)
            self.logger.addHandler(handler)

    def setup_environment(self) -> dict:
        """
        Create environment variables for UWP Wine session

        Returns:
            Dictionary of environment variables
        """
        env = dict(os.environ)

        libdir = self.dist_path / 'lib'

        # Library paths
        lib_paths = [
            str(libdir / 'x86_64-linux-gnu'),
            str(libdir / 'i386-linux-gnu'),
        ]

        env['LD_LIBRARY_PATH'] = ':'.join(lib_paths)
        env['WINEPREFIX'] = str(self.prefix_path)
        env['WINEDLLPATH'] = str(libdir / 'vkd3d')

        # Wine DLL overrides for UWP
        dll_overrides = []

        # Set WinRT DLLs to builtin
        for dll in self.WINRT_DLLS:
            dll_overrides.append(f"{dll}=b")

        # Set support DLLs to builtin,native
        for dll in self.UWP_SUPPORT_DLLS:
            dll_overrides.append(f"{dll}=b,n")

        if dll_overrides:
            env['WINEDLLOVERRIDES'] = ';'.join(dll_overrides)

        # Enable Wine debugging for UWP (can be disabled in production)
        # env['WINEDEBUG'] = '+winrt,+combase,+ole,+rpc'

        return env

    def create_uwp_directories(self):
        """Create UWP-specific directory structure in Wine prefix"""
        drive_c = self.prefix_path / 'drive_c'

        # Create WindowsApps directory (where UWP apps are installed)
        windows_apps = drive_c / 'Program Files' / 'WindowsApps'
        windows_apps.mkdir(parents=True, exist_ok=True)

        # Create user's app data directories
        local_app_data = drive_c / 'users' / os.getenv('USER', 'steamuser') / 'AppData' / 'Local'
        local_app_data.mkdir(parents=True, exist_ok=True)

        # Packages directory for UWP app data
        packages_dir = local_app_data / 'Packages'
        packages_dir.mkdir(parents=True, exist_ok=True)

        self.logger.info(f"Created UWP directories in prefix")

        return {
            'windows_apps': windows_apps,
            'packages': packages_dir,
            'local_app_data': local_app_data,
        }

    def run_wine_command(self, command: list, env: Optional[dict] = None) -> int:
        """
        Run a Wine command with proper environment

        Args:
            command: Command to run
            env: Environment variables (uses setup_environment() if None)

        Returns:
            Return code
        """
        if env is None:
            env = self.setup_environment()

        wine_bin = self.dist_path / 'bin' / 'wine'

        full_command = [str(wine_bin)] + command

        self.logger.debug(f"Running: {' '.join(full_command)}")

        result = subprocess.run(
            full_command,
            env=env,
            capture_output=True,
            text=True
        )

        if result.returncode != 0:
            self.logger.error(f"Command failed with code {result.returncode}")
            if result.stderr:
                self.logger.error(f"STDERR: {result.stderr}")

        return result.returncode

    def setup_registry(self):
        """Configure registry for UWP support"""
        env = self.setup_environment()

        # Create registry entries for UWP
        reg_commands = [
            # Windows version (set to Windows 10)
            r'reg add "HKLM\Software\Microsoft\Windows NT\CurrentVersion" /v CurrentVersion /t REG_SZ /d "10.0" /f',
            r'reg add "HKLM\Software\Microsoft\Windows NT\CurrentVersion" /v CurrentBuild /t REG_SZ /d "19045" /f',
            r'reg add "HKLM\Software\Microsoft\Windows NT\CurrentVersion" /v CurrentBuildNumber /t REG_SZ /d "19045" /f',

            # UWP-related registry keys
            r'reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\AppModel" /f',
            r'reg add "HKCU\Software\Classes\Local Settings" /f',
        ]

        for reg_cmd in reg_commands:
            self.logger.debug(f"Registry: {reg_cmd}")
            self.run_wine_command(['cmd', '/c', reg_cmd], env)

    def initialize_prefix(self, force: bool = False):
        """
        Initialize Wine prefix for UWP

        Args:
            force: Force re-initialization
        """
        if self.prefix_path.exists() and not force:
            self.logger.info(f"Prefix already exists: {self.prefix_path}")
            self.logger.info("Use force=True to re-initialize")
            return

        self.logger.info(f"Initializing UWP prefix: {self.prefix_path}")

        env = self.setup_environment()
        env['WINEDEBUG'] = '-all'  # Suppress output during initialization

        # Create prefix directory
        self.prefix_path.mkdir(parents=True, exist_ok=True)

        # Run wineboot to create the prefix
        wine_bin = self.dist_path / 'bin' / 'wine'
        wineserver_bin = self.dist_path / 'bin' / 'wineserver'

        self.logger.info("Creating Wine prefix...")
        subprocess.run(
            [str(wine_bin), 'wineboot'],
            env=env,
            check=True,
            capture_output=True
        )

        # Wait for wineserver to finish
        subprocess.run(
            [str(wineserver_bin), '-w'],
            env=env,
            check=True,
            capture_output=True
        )

        # Create UWP directories
        self.create_uwp_directories()

        # Setup registry
        self.setup_registry()

        self.logger.info("UWP prefix initialized successfully")

    def install_uwp_app(self, package_dir: Path, app_name: Optional[str] = None):
        """
        Install a UWP app into the prefix

        Args:
            package_dir: Extracted UWP package directory
            app_name: Application name (uses package_dir name if None)
        """
        if app_name is None:
            app_name = package_dir.name

        dirs = self.create_uwp_directories()
        windows_apps = dirs['windows_apps']

        # Create symlink or copy package to WindowsApps
        target_dir = windows_apps / app_name

        if target_dir.exists():
            self.logger.warning(f"App already installed: {target_dir}")
            return target_dir

        # Create symlink to the extracted package
        try:
            # Use relative symlink if possible
            import os
            target_dir.symlink_to(os.path.abspath(package_dir))
            self.logger.info(f"Installed UWP app: {app_name}")
        except Exception as e:
            self.logger.error(f"Failed to install app: {e}")
            # Fallback: copy the directory
            import shutil
            shutil.copytree(package_dir, target_dir)
            self.logger.info(f"Copied UWP app: {app_name}")

        return target_dir

    def get_wine_binary(self) -> Path:
        """Get path to Wine binary"""
        return self.dist_path / 'bin' / 'wine'

    def get_wine64_binary(self) -> Path:
        """Get path to Wine64 binary"""
        wine64 = self.dist_path / 'bin' / 'wine64'
        if wine64.exists():
            return wine64
        return self.get_wine_binary()


def main():
    """Command-line interface"""
    import sys
    import argparse

    parser = argparse.ArgumentParser(description='Setup Wine prefix for UWP')
    parser.add_argument('prefix', help='Path to Wine prefix')
    parser.add_argument('dist', help='Path to Proton dist directory')
    parser.add_argument('--force', action='store_true',
                       help='Force re-initialization')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Verbose output')

    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO

    setup = UWPPrefixSetup(
        Path(args.prefix),
        Path(args.dist),
        log_level=log_level
    )

    try:
        setup.initialize_prefix(force=args.force)
        print(f"UWP prefix ready: {args.prefix}")
        return 0
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
