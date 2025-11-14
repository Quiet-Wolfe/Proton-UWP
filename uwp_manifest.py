#!/usr/bin/env python3

"""
UWP AppxManifest.xml Parser
Parses UWP/APPX package manifests to extract application metadata
"""

import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Optional


class AppxManifest:
    """Parser for AppxManifest.xml files"""

    # Common XML namespaces in APPX manifests
    NAMESPACES = {
        'default': 'http://schemas.microsoft.com/appx/manifest/foundation/windows10',
        'uap': 'http://schemas.microsoft.com/appx/manifest/uap/windows10',
        'uap2': 'http://schemas.microsoft.com/appx/manifest/uap/windows10/2',
        'uap3': 'http://schemas.microsoft.com/appx/manifest/uap/windows10/3',
        'uap4': 'http://schemas.microsoft.com/appx/manifest/uap/windows10/4',
        'uap5': 'http://schemas.microsoft.com/appx/manifest/uap/windows10/5',
        'uap6': 'http://schemas.microsoft.com/appx/manifest/uap/windows10/6',
        'rescap': 'http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities',
        'desktop': 'http://schemas.microsoft.com/appx/manifest/desktop/windows10',
        # Windows 8.x namespaces (legacy)
        'w8': 'http://schemas.microsoft.com/appx/2010/manifest',
        'w81': 'http://schemas.microsoft.com/appx/2013/manifest',
        # Build/Store namespaces
        'build': 'http://schemas.microsoft.com/developer/appx/2015/build',
        'mp': 'http://schemas.microsoft.com/appx/2014/phone/manifest',
    }

    def __init__(self, manifest_path: Path):
        """
        Initialize manifest parser

        Args:
            manifest_path: Path to AppxManifest.xml file
        """
        self.manifest_path = manifest_path
        self.tree = ET.parse(manifest_path)
        self.root = self.tree.getroot()

        # Auto-detect and register namespaces
        self._register_namespaces()

    def _register_namespaces(self):
        """Auto-detect and register XML namespaces from the manifest"""
        # Register predefined namespaces
        for prefix, uri in self.NAMESPACES.items():
            ET.register_namespace(prefix, uri)

        # Also try to extract namespaces from the root element
        for prefix, uri in self.root.attrib.items():
            if prefix.startswith('{http://www.w3.org/2000/xmlns/}'):
                ns_prefix = prefix.split('}')[1]
                if ns_prefix and ns_prefix not in self.NAMESPACES:
                    self.NAMESPACES[ns_prefix] = uri

    def _find_element(self, xpath: str, namespaces: Optional[Dict] = None) -> Optional[ET.Element]:
        """Find first matching element using XPath"""
        if namespaces is None:
            namespaces = self.NAMESPACES
        return self.root.find(xpath, namespaces)

    def _find_all_elements(self, xpath: str, namespaces: Optional[Dict] = None) -> List[ET.Element]:
        """Find all matching elements using XPath"""
        if namespaces is None:
            namespaces = self.NAMESPACES
        return self.root.findall(xpath, namespaces)

    def get_identity(self) -> Dict[str, str]:
        """Get package identity information"""
        # Try Windows 10 schema
        identity = self._find_element('.//default:Identity')
        if identity is None:
            # Try Windows 8.x schema
            identity = self._find_element('.//w8:Identity')
        if identity is None:
            identity = self._find_element('.//w81:Identity')

        if identity is not None:
            return {
                'name': identity.get('Name', ''),
                'publisher': identity.get('Publisher', ''),
                'version': identity.get('Version', ''),
                'processor_architecture': identity.get('ProcessorArchitecture', 'neutral'),
            }
        return {}

    def get_properties(self) -> Dict[str, str]:
        """Get package properties"""
        props = self._find_element('.//default:Properties') or self._find_element('.//w8:Properties')
        if props is None:
            return {}

        result = {}

        # Display name
        display_name = props.find('.//default:DisplayName', self.NAMESPACES)
        if display_name is None:
            display_name = props.find('.//w8:DisplayName', self.NAMESPACES)
        if display_name is not None:
            result['display_name'] = display_name.text or ''

        # Publisher display name
        pub_name = props.find('.//default:PublisherDisplayName', self.NAMESPACES)
        if pub_name is None:
            pub_name = props.find('.//w8:PublisherDisplayName', self.NAMESPACES)
        if pub_name is not None:
            result['publisher_display_name'] = pub_name.text or ''

        # Description
        desc = props.find('.//default:Description', self.NAMESPACES)
        if desc is None:
            desc = props.find('.//w8:Description', self.NAMESPACES)
        if desc is not None:
            result['description'] = desc.text or ''

        return result

    def get_applications(self) -> List[Dict[str, str]]:
        """Get list of applications defined in the package"""
        apps = []

        # Try different schemas
        app_elements = (
            self._find_all_elements('.//default:Applications/default:Application') or
            self._find_all_elements('.//w8:Applications/w8:Application')
        )

        for app in app_elements:
            app_info = {
                'id': app.get('Id', ''),
                'executable': app.get('Executable', ''),
                'entry_point': app.get('EntryPoint', ''),
            }

            # Get visual elements for display name
            visual = app.find('.//uap:VisualElements', self.NAMESPACES)
            if visual is None:
                visual = app.find('.//w8:VisualElements', self.NAMESPACES)
            if visual is not None:
                app_info['display_name'] = visual.get('DisplayName', '')
                app_info['description'] = visual.get('Description', '')
                app_info['background_color'] = visual.get('BackgroundColor', '')

            apps.append(app_info)

        return apps

    def get_dependencies(self) -> List[Dict[str, str]]:
        """Get package dependencies"""
        deps = []

        # Package dependencies
        dep_elements = self._find_all_elements('.//default:Dependencies/default:PackageDependency')
        if not dep_elements:
            dep_elements = self._find_all_elements('.//w8:Dependencies/w8:PackageDependency')

        for dep in dep_elements:
            deps.append({
                'name': dep.get('Name', ''),
                'publisher': dep.get('Publisher', ''),
                'min_version': dep.get('MinVersion', ''),
            })

        return deps

    def get_capabilities(self) -> List[str]:
        """Get requested capabilities"""
        caps = []

        # Regular capabilities
        cap_elements = self._find_all_elements('.//default:Capabilities/default:Capability')
        if not cap_elements:
            cap_elements = self._find_all_elements('.//w8:Capabilities/w8:Capability')

        for cap in cap_elements:
            name = cap.get('Name', '')
            if name:
                caps.append(name)

        # Device capabilities
        dev_caps = self._find_all_elements('.//default:Capabilities/default:DeviceCapability')
        if not dev_caps:
            dev_caps = self._find_all_elements('.//w8:Capabilities/w8:DeviceCapability')

        for cap in dev_caps:
            name = cap.get('Name', '')
            if name:
                caps.append(f"Device:{name}")

        # Restricted capabilities
        res_caps = self._find_all_elements('.//default:Capabilities/rescap:Capability')
        for cap in res_caps:
            name = cap.get('Name', '')
            if name:
                caps.append(f"Restricted:{name}")

        return caps

    def get_target_device_family(self) -> List[Dict[str, str]]:
        """Get target device family information"""
        families = []

        dep_elem = self._find_element('.//default:Dependencies')
        if dep_elem is None:
            dep_elem = self._find_element('.//w8:Dependencies')

        if dep_elem is not None:
            family_elements = dep_elem.findall('.//default:TargetDeviceFamily', self.NAMESPACES)
            if not family_elements:
                family_elements = dep_elem.findall('.//w8:TargetDeviceFamily', self.NAMESPACES)

            for family in family_elements:
                families.append({
                    'name': family.get('Name', ''),
                    'min_version': family.get('MinVersion', ''),
                    'max_version_tested': family.get('MaxVersionTested', ''),
                })

        return families

    def get_main_executable(self) -> Optional[str]:
        """Get the main executable path"""
        apps = self.get_applications()
        if apps:
            # Return the first application's executable
            return apps[0].get('executable')
        return None

    def to_dict(self) -> Dict:
        """Convert manifest to dictionary representation"""
        return {
            'identity': self.get_identity(),
            'properties': self.get_properties(),
            'applications': self.get_applications(),
            'dependencies': self.get_dependencies(),
            'capabilities': self.get_capabilities(),
            'target_device_families': self.get_target_device_family(),
        }

    def __str__(self) -> str:
        """String representation"""
        identity = self.get_identity()
        props = self.get_properties()
        return (
            f"UWP Package: {props.get('display_name', 'Unknown')}\n"
            f"  Name: {identity.get('name', 'Unknown')}\n"
            f"  Version: {identity.get('version', 'Unknown')}\n"
            f"  Publisher: {props.get('publisher_display_name', 'Unknown')}\n"
            f"  Architecture: {identity.get('processor_architecture', 'Unknown')}"
        )


if __name__ == '__main__':
    import sys
    import json

    if len(sys.argv) < 2:
        print("Usage: uwp_manifest.py <path_to_AppxManifest.xml>")
        sys.exit(1)

    manifest_path = Path(sys.argv[1])
    if not manifest_path.exists():
        print(f"Error: {manifest_path} not found")
        sys.exit(1)

    try:
        manifest = AppxManifest(manifest_path)
        print(manifest)
        print("\n" + "="*60 + "\n")
        print(json.dumps(manifest.to_dict(), indent=2))
    except Exception as e:
        print(f"Error parsing manifest: {e}")
        sys.exit(1)
