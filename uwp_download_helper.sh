#!/bin/bash

# UWP Package Download Helper
# Helps download UWP packages from Microsoft Store

set -e

echo "=========================================="
echo "  Proton UWP - Package Download Helper"
echo "=========================================="
echo ""

# Check if the user provided a product ID or URL
if [ $# -lt 1 ]; then
    cat <<EOF
Usage: $0 <Microsoft Store URL or Product ID>

Examples:
  $0 https://www.microsoft.com/store/productId/9NCBL78CG9N7
  $0 9NCBL78CG9N7

This script will help you download UWP packages using web-based tools.

Popular UWP Games Product IDs:
  - Valheim: 9NCBL78CG9N7
  - Minecraft for Windows: 9NBLGGH2JHXJ
  - Forza Horizon 4: 9PNJXVCVWD4K
  - Sea of Thieves: 9P2N57MC619K

Note: You'll need to manually download the files from the provided URL.
EOF
    exit 1
fi

INPUT="$1"

# Extract Product ID if a URL was provided
if [[ "$INPUT" == *"microsoft.com"* ]]; then
    # Try to extract product ID from URL
    if [[ "$INPUT" =~ ([0-9][A-Z0-9]{11}) ]]; then
        PRODUCT_ID="${BASH_REMATCH[1]}"
        echo "Extracted Product ID: $PRODUCT_ID"
    else
        echo "Error: Could not extract Product ID from URL"
        exit 1
    fi
else
    PRODUCT_ID="$INPUT"
fi

echo ""
echo "Product ID: $PRODUCT_ID"
echo ""
echo "=========================================="
echo "  Download Instructions"
echo "=========================================="
echo ""
echo "To download UWP packages for this app:"
echo ""
echo "1. Go to Adguard Store:"
echo "   https://store.rg-adguard.net/"
echo ""
echo "2. In the URL field, paste:"
echo "   $PRODUCT_ID"
echo ""
echo "3. Select 'ProductId' as the type"
echo "4. Select 'Retail' as the channel"
echo "5. Click the checkmark button"
echo ""
echo "6. Download these files:"
echo "   - The main .appx/.msix file (look for x64 architecture)"
echo "   - Any dependency .appx files (Microsoft.VCLibs, .NET, etc.)"
echo ""
echo "7. Save all files to a directory, for example:"
echo "   mkdir -p ~/uwp_packages/$PRODUCT_ID"
echo "   # Download files to that directory"
echo ""
echo "8. Launch with Proton UWP:"
echo "   ./proton_uwp ~/uwp_packages/$PRODUCT_ID/MainApp.appx"
echo ""
echo "=========================================="
echo "  Alternative: Command Line Download"
echo "=========================================="
echo ""
echo "You can also use curl or wget to download directly:"
echo ""
echo "# First, visit the Adguard Store link above to get the download URLs"
echo "# Then use curl/wget to download each file:"
echo "# curl -L -o Package.appx 'https://...download-url...'"
echo ""
echo "=========================================="
echo ""
echo "Opening Adguard Store in your browser (if available)..."
echo ""

# Try to open browser
ADGUARD_URL="https://store.rg-adguard.net/"

if command -v xdg-open &> /dev/null; then
    xdg-open "$ADGUARD_URL" 2>/dev/null || true
elif command -v open &> /dev/null; then
    open "$ADGUARD_URL" 2>/dev/null || true
elif command -v firefox &> /dev/null; then
    firefox "$ADGUARD_URL" 2>/dev/null &
elif command -v google-chrome &> /dev/null; then
    google-chrome "$ADGUARD_URL" 2>/dev/null &
else
    echo "Could not detect a browser. Please manually visit:"
    echo "$ADGUARD_URL"
fi

echo ""
echo "Product ID: $PRODUCT_ID"
echo "Paste this into the search box and follow the instructions above."
echo ""
