# Quick Start Guide - Proton UWP

Get Valheim (or other UWP apps) running in 5 minutes!

## Step 1: Build Proton (if not already built)

```bash
cd Proton-UWP
make install
```

This takes a while (30-60 minutes depending on your system). Go grab a coffee! ☕

## Step 2: Download Valheim Package

### Option A: Use the Helper Script

```bash
./uwp_download_helper.sh 9NCBL78CG9N7
```

This will open Adguard Store in your browser. Then:

1. The Product ID `9NCBL78CG9N7` will be shown
2. Paste it into the search box on the website
3. Select "ProductId" and "Retail"
4. Click the checkmark
5. Download the x64 .appx/.msix file (usually the largest one)
6. Download any dependency packages (Microsoft.VCLibs.*.appx, etc.)

### Option B: Manual Download

1. Visit: https://store.rg-adguard.net/
2. Enter: `9NCBL78CG9N7` (Valheim's Product ID)
3. Select: ProductId + Retail
4. Download the main package and dependencies

Save all files to a folder like `~/valheim_uwp/`

## Step 3: Launch Valheim

```bash
./proton_uwp ~/valheim_uwp/Valheim_*.appx
```

That's it! The launcher will:
- Extract the package
- Set up a Wine prefix with UWP support
- Configure DirectX translation (DXVK)
- Launch the game

## Step 4: Troubleshooting

If it doesn't work:

```bash
# Enable detailed logging
PROTON_LOG=1 ./proton_uwp ~/valheim_uwp/Valheim_*.appx

# Check the log
less ~/proton_uwp.log
```

Look for error messages about missing DLLs or WinRT APIs.

## Common Issues

### "Could not find Proton dist directory"

Proton isn't built yet. Run `make install` first.

### "Not a valid UWP package"

You may have downloaded the wrong file. Make sure you get the .appx or .msix file, not a .zip or .appxbundle without extracting it.

### Game crashes immediately

- Check that you downloaded all dependencies
- Verify your GPU supports Vulkan (required for DXVK)
- Try with `PROTON_USE_WINED3D=1` to use OpenGL instead

### Black screen

Give it time - first launch can take a while as Wine sets up. Wait 30-60 seconds.

## Next Steps

- Read the full documentation: [README-UWP.md](README-UWP.md)
- Try other UWP games
- Report issues and successes!

## Performance Tips

### For Best Performance:

```bash
# Use DXVK (default, requires Vulkan)
./proton_uwp Valheim.appx

# Enable fsync if your kernel supports it
PROTON_NO_FSYNC=0 ./proton_uwp Valheim.appx
```

### For Compatibility:

```bash
# Fall back to OpenGL if DXVK has issues
PROTON_USE_WINED3D=1 ./proton_uwp Valheim.appx
```

## Testing Different Apps

```bash
# Valheim
./uwp_download_helper.sh 9NCBL78CG9N7

# Minecraft for Windows 10
./uwp_download_helper.sh 9NBLGGH2JHXJ

# Any Microsoft Store URL
./uwp_download_helper.sh https://www.microsoft.com/store/productId/YOUR_PRODUCT_ID
```

---

**Have fun!** Remember, this is experimental. Not all apps will work, but when they do, it's pretty cool! 🎮
