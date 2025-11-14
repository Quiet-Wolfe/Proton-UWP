#!/usr/bin/env python3

"""
UWP Package Extractor
Extracts APPX/MSIX packages and handles dependencies
"""

import os
import sys
import zipfile
import shutil
from pathlib import Path
from typing import List, Optional, Set
import logging

from uwp_manifest import AppxManifest


class UWPPackageExtractor:
    """Extract and manage UWP packages"""

    def __init__(self, log_level=logging.INFO):
        """
        Initialize the UWP package extractor

        Args:
            log_level: Logging level
        """
        self.logger = logging.getLogger('UWPExtractor')
        self.logger.setLevel(log_level)

        if not self.logger.handlers:
            handler = logging.StreamHandler()
            formatter = logging.Formatter('[%(levelname)s] %(message)s')
            handler.setFormatter(formatter)
            self.logger.addHandler(handler)

    def is_uwp_package(self, package_path: Path) -> bool:
        """
        Check if a file is a valid UWP package

        Args:
            package_path: Path to potential package file

        Returns:
            True if valid UWP package
        """
        if not package_path.exists():
            return False

        # Check file extension
        if package_path.suffix.lower() not in ['.appx', '.msix', '.appxbundle', '.msixbundle']:
            return False

        # Check if it's a valid ZIP file
        if not zipfile.is_zipfile(package_path):
            return False

        # Check for AppxManifest.xml
        try:
            with zipfile.ZipFile(package_path, 'r') as zf:
                return 'AppxManifest.xml' in zf.namelist()
        except:
            return False

    def extract_package(self, package_path: Path, output_dir: Path, overwrite: bool = False) -> Optional[Path]:
        """
        Extract a UWP package

        Args:
            package_path: Path to APPX/MSIX file
            output_dir: Directory to extract to
            overwrite: Overwrite existing extraction

        Returns:
            Path to extracted package directory
        """
        if not self.is_uwp_package(package_path):
            self.logger.error(f"Not a valid UWP package: {package_path}")
            return None

        # Create output directory
        package_name = package_path.stem
        extract_path = output_dir / package_name

        if extract_path.exists():
            if overwrite:
                self.logger.info(f"Removing existing extraction: {extract_path}")
                shutil.rmtree(extract_path)
            else:
                self.logger.info(f"Package already extracted: {extract_path}")
                return extract_path

        self.logger.info(f"Extracting {package_path.name} to {extract_path}")

        try:
            extract_path.mkdir(parents=True, exist_ok=True)

            with zipfile.ZipFile(package_path, 'r') as zf:
                # Extract all files
                zf.extractall(extract_path)

                # List extracted files for debugging
                file_count = len(zf.namelist())
                self.logger.debug(f"Extracted {file_count} files")

            return extract_path

        except Exception as e:
            self.logger.error(f"Failed to extract package: {e}")
            if extract_path.exists():
                shutil.rmtree(extract_path)
            return None

    def extract_bundle(self, bundle_path: Path, output_dir: Path, architecture: str = 'x64') -> Optional[List[Path]]:
        """
        Extract an APPX/MSIX bundle and select appropriate packages

        Args:
            bundle_path: Path to bundle file
            output_dir: Directory to extract to
            architecture: Preferred architecture (x64, x86, arm64)

        Returns:
            List of extracted package directories
        """
        if bundle_path.suffix.lower() not in ['.appxbundle', '.msixbundle']:
            self.logger.error(f"Not a bundle file: {bundle_path}")
            return None

        if not zipfile.is_zipfile(bundle_path):
            self.logger.error(f"Invalid bundle file: {bundle_path}")
            return None

        self.logger.info(f"Extracting bundle: {bundle_path.name}")

        temp_extract = output_dir / f"_bundle_{bundle_path.stem}"
        temp_extract.mkdir(parents=True, exist_ok=True)

        extracted_packages = []

        try:
            # Extract the bundle
            with zipfile.ZipFile(bundle_path, 'r') as zf:
                zf.extractall(temp_extract)

            # Find and extract individual packages
            for item in temp_extract.rglob('*.appx'):
                if self._should_extract_package(item, architecture):
                    extracted = self.extract_package(item, output_dir)
                    if extracted:
                        extracted_packages.append(extracted)

            for item in temp_extract.rglob('*.msix'):
                if self._should_extract_package(item, architecture):
                    extracted = self.extract_package(item, output_dir)
                    if extracted:
                        extracted_packages.append(extracted)

            return extracted_packages

        except Exception as e:
            self.logger.error(f"Failed to extract bundle: {e}")
            return None

        finally:
            # Clean up temporary bundle extraction
            if temp_extract.exists():
                shutil.rmtree(temp_extract)

    def _should_extract_package(self, package_path: Path, preferred_arch: str) -> bool:
        """
        Determine if a package should be extracted based on architecture

        Args:
            package_path: Path to package
            preferred_arch: Preferred architecture

        Returns:
            True if package should be extracted
        """
        # Simple heuristic: check filename for architecture
        name_lower = package_path.name.lower()

        # Always extract neutral/anycpu packages
        if 'neutral' in name_lower or 'anycpu' in name_lower:
            return True

        # Architecture-specific
        arch_map = {
            'x64': ['x64', 'amd64'],
            'x86': ['x86', 'win32'],
            'arm64': ['arm64'],
            'arm': ['arm'],
        }

        preferred_patterns = arch_map.get(preferred_arch.lower(), [])
        for pattern in preferred_patterns:
            if pattern in name_lower:
                return True

        # If no architecture found in name, try to extract and check manifest
        return False

    def get_package_info(self, package_path: Path) -> Optional[dict]:
        """
        Get information about a UWP package without extracting

        Args:
            package_path: Path to package

        Returns:
            Package information dictionary
        """
        if not self.is_uwp_package(package_path):
            return None

        try:
            with zipfile.ZipFile(package_path, 'r') as zf:
                # Read manifest
                manifest_data = zf.read('AppxManifest.xml')

                # Write to temp file for parsing
                import tempfile
                with tempfile.NamedTemporaryFile(mode='wb', suffix='.xml', delete=False) as f:
                    f.write(manifest_data)
                    temp_path = f.name

                try:
                    manifest = AppxManifest(Path(temp_path))
                    info = manifest.to_dict()
                    info['package_file'] = str(package_path)
                    return info
                finally:
                    os.unlink(temp_path)

        except Exception as e:
            self.logger.error(f"Failed to get package info: {e}")
            return None

    def find_dependencies(self, package_dir: Path) -> List[str]:
        """
        Find dependencies from an extracted package

        Args:
            package_dir: Extracted package directory

        Returns:
            List of dependency names
        """
        manifest_path = package_dir / 'AppxManifest.xml'
        if not manifest_path.exists():
            return []

        try:
            manifest = AppxManifest(manifest_path)
            deps = manifest.get_dependencies()
            return [dep['name'] for dep in deps]
        except Exception as e:
            self.logger.error(f"Failed to read dependencies: {e}")
            return []

    def find_main_executable(self, package_dir: Path) -> Optional[Path]:
        """
        Find the main executable in an extracted package

        Args:
            package_dir: Extracted package directory

        Returns:
            Path to main executable
        """
        manifest_path = package_dir / 'AppxManifest.xml'
        if not manifest_path.exists():
            return None

        try:
            manifest = AppxManifest(manifest_path)
            exe_path = manifest.get_main_executable()

            if exe_path:
                # Convert Windows path to Unix path
                exe_path = exe_path.replace('\\', '/')
                full_path = package_dir / exe_path

                if full_path.exists():
                    return full_path
                else:
                    self.logger.warning(f"Executable not found: {full_path}")

            return None

        except Exception as e:
            self.logger.error(f"Failed to find main executable: {e}")
            return None

    def create_installation(self, package_paths: List[Path], install_dir: Path,
                           architecture: str = 'x64') -> Optional[Path]:
        """
        Create a complete UWP installation from package(s) and dependencies

        Args:
            package_paths: List of package/bundle files
            install_dir: Installation directory
            architecture: Preferred architecture

        Returns:
            Path to main package directory
        """
        self.logger.info(f"Creating UWP installation in {install_dir}")

        install_dir.mkdir(parents=True, exist_ok=True)
        extracted_packages = []

        # Extract all packages
        for pkg_path in package_paths:
            pkg_path = Path(pkg_path)

            if pkg_path.suffix.lower() in ['.appxbundle', '.msixbundle']:
                # Extract bundle
                bundle_packages = self.extract_bundle(pkg_path, install_dir, architecture)
                if bundle_packages:
                    extracted_packages.extend(bundle_packages)
            else:
                # Extract single package
                extracted = self.extract_package(pkg_path, install_dir)
                if extracted:
                    extracted_packages.append(extracted)

        if not extracted_packages:
            self.logger.error("No packages were extracted")
            return None

        # Find the main package (first non-dependency package)
        main_package = extracted_packages[0]

        self.logger.info(f"Main package: {main_package.name}")
        self.logger.info(f"Total packages extracted: {len(extracted_packages)}")

        return main_package


def main():
    """Command-line interface"""
    import argparse

    parser = argparse.ArgumentParser(description='Extract UWP/APPX packages')
    parser.add_argument('package', help='Path to APPX/MSIX file or bundle')
    parser.add_argument('-o', '--output', default='uwp_extracted',
                       help='Output directory (default: uwp_extracted)')
    parser.add_argument('-a', '--arch', default='x64',
                       choices=['x64', 'x86', 'arm64'],
                       help='Preferred architecture (default: x64)')
    parser.add_argument('--overwrite', action='store_true',
                       help='Overwrite existing extraction')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Verbose output')
    parser.add_argument('--info', action='store_true',
                       help='Show package info without extracting')

    args = parser.parse_args()

    log_level = logging.DEBUG if args.verbose else logging.INFO
    extractor = UWPPackageExtractor(log_level=log_level)

    package_path = Path(args.package)
    output_dir = Path(args.output)

    if not package_path.exists():
        print(f"Error: Package not found: {package_path}")
        return 1

    if args.info:
        # Just show info
        info = extractor.get_package_info(package_path)
        if info:
            import json
            print(json.dumps(info, indent=2))
            return 0
        else:
            print("Failed to get package info")
            return 1

    # Extract package
    result = extractor.create_installation([package_path], output_dir, args.arch)

    if result:
        print(f"\nExtraction successful!")
        print(f"Main package: {result}")

        # Find and display main executable
        exe = extractor.find_main_executable(result)
        if exe:
            print(f"Main executable: {exe}")

        return 0
    else:
        print("Extraction failed")
        return 1


if __name__ == '__main__':
    sys.exit(main())
