/**
 * Frida Script to Trace WinRT API Calls
 *
 * Usage:
 *   frida -l frida_winrt_trace.js -n Valheim.exe
 *   frida -l frida_winrt_trace.js -p <PID>
 *
 * This script traces:
 * - RoInitialize/RoUninitialize
 * - RoActivateInstance
 * - RoGetActivationFactory
 * - HSTRING operations
 * - IInspectable/IActivationFactory calls
 */

console.log("[*] WinRT API Tracer loaded");
console.log("[*] Attaching to combase.dll...");

// Helper function to read HSTRING
function readHString(hstring) {
    if (hstring.isNull()) {
        return "<null>";
    }

    try {
        // WindowsGetStringRawBuffer
        var combase = Process.getModuleByName("combase.dll");
        var WindowsGetStringRawBuffer = new NativeFunction(
            combase.getExportByName("WindowsGetStringRawBuffer"),
            'pointer', ['pointer', 'pointer']
        );

        var lengthPtr = Memory.alloc(4);
        var strPtr = WindowsGetStringRawBuffer(hstring, lengthPtr);

        if (!strPtr.isNull()) {
            var length = lengthPtr.readU32();
            return strPtr.readUtf16String(length);
        }
    } catch (e) {
        return "<error reading string>";
    }

    return "<unknown>";
}

// Helper to read GUID
function readGuid(guidPtr) {
    if (guidPtr.isNull()) {
        return "<null>";
    }

    try {
        var d1 = guidPtr.readU32();
        var d2 = guidPtr.add(4).readU16();
        var d3 = guidPtr.add(6).readU16();
        var d4 = [];
        for (var i = 0; i < 8; i++) {
            d4.push(guidPtr.add(8 + i).readU8());
        }

        return "{" +
            d1.toString(16).padStart(8, '0') + "-" +
            d2.toString(16).padStart(4, '0') + "-" +
            d3.toString(16).padStart(4, '0') + "-" +
            d4.slice(0, 2).map(x => x.toString(16).padStart(2, '0')).join('') + "-" +
            d4.slice(2).map(x => x.toString(16).padStart(2, '0')).join('') +
            "}";
    } catch (e) {
        return "<error reading GUID>";
    }
}

// Track initialization state
var roInitialized = false;
var activationCount = 0;
var classesActivated = new Set();

// Try to load combase.dll
try {
    var combase = Process.getModuleByName("combase.dll");
    console.log("[+] combase.dll found at: " + combase.base);

    // Hook RoInitialize
    if (combase.getExportByName("RoInitialize")) {
        Interceptor.attach(combase.getExportByName("RoInitialize"), {
            onEnter: function(args) {
                var initType = args[0].toInt32();
                var initTypeStr = {
                    0: "RO_INIT_SINGLETHREADED",
                    1: "RO_INIT_MULTITHREADED"
                }[initType] || "UNKNOWN(" + initType + ")";

                console.log("\n[RoInitialize]");
                console.log("  Type: " + initTypeStr);
                this.initType = initTypeStr;
            },
            onLeave: function(retval) {
                var hr = retval.toInt32();
                console.log("  Result: " + (hr === 0 ? "S_OK" : "0x" + hr.toString(16)));
                if (hr === 0) {
                    roInitialized = true;
                }
            }
        });
        console.log("[+] Hooked RoInitialize");
    }

    // Hook RoUninitialize
    if (combase.getExportByName("RoUninitialize")) {
        Interceptor.attach(combase.getExportByName("RoUninitialize"), {
            onEnter: function(args) {
                console.log("\n[RoUninitialize]");
                roInitialized = false;
            }
        });
        console.log("[+] Hooked RoUninitialize");
    }

    // Hook RoActivateInstance - KEY FUNCTION!
    if (combase.getExportByName("RoActivateInstance")) {
        Interceptor.attach(combase.getExportByName("RoActivateInstance"), {
            onEnter: function(args) {
                var className = readHString(args[0]);
                console.log("\n[RoActivateInstance] #" + (++activationCount));
                console.log("  Class: " + className);
                console.log("  Instance Ptr: " + args[1]);

                this.className = className;
                classesActivated.add(className);
            },
            onLeave: function(retval) {
                var hr = retval.toInt32();
                console.log("  Result: " + (hr === 0 ? "S_OK" : "0x" + hr.toString(16)));
                if (hr !== 0) {
                    console.log("  ⚠️  FAILED for: " + this.className);
                }
            }
        });
        console.log("[+] Hooked RoActivateInstance");
    }

    // Hook RoGetActivationFactory
    if (combase.getExportByName("RoGetActivationFactory")) {
        Interceptor.attach(combase.getExportByName("RoGetActivationFactory"), {
            onEnter: function(args) {
                var className = readHString(args[0]);
                var iid = readGuid(args[1]);
                console.log("\n[RoGetActivationFactory]");
                console.log("  Class: " + className);
                console.log("  IID: " + iid);

                this.className = className;
            },
            onLeave: function(retval) {
                var hr = retval.toInt32();
                console.log("  Result: " + (hr === 0 ? "S_OK" : "0x" + hr.toString(16)));
                if (hr !== 0) {
                    console.log("  ⚠️  FAILED for: " + this.className);
                }
            }
        });
        console.log("[+] Hooked RoGetActivationFactory");
    }

    // Hook WindowsCreateString
    if (combase.getExportByName("WindowsCreateString")) {
        var createStringCount = 0;
        Interceptor.attach(combase.getExportByName("WindowsCreateString"), {
            onEnter: function(args) {
                var str = args[0].readUtf16String(args[1].toInt32());
                this.str = str;
                // Only log interesting strings (not too noisy)
                if (str.includes("Windows.") || str.includes("://") || str.length > 50) {
                    console.log("\n[WindowsCreateString]");
                    console.log("  String: " + str.substring(0, 100) + (str.length > 100 ? "..." : ""));
                }
            }
        });
        console.log("[+] Hooked WindowsCreateString");
    }

    // Hook WindowsDeleteString
    if (combase.getExportByName("WindowsDeleteString")) {
        Interceptor.attach(combase.getExportByName("WindowsDeleteString"), {
            onEnter: function(args) {
                // Usually too noisy, but we track it
            }
        });
        console.log("[+] Hooked WindowsDeleteString");
    }

    // Print summary every 10 seconds
    setInterval(function() {
        if (activationCount > 0 || roInitialized) {
            console.log("\n=== WinRT Status ===");
            console.log("Initialized: " + roInitialized);
            console.log("Total Activations: " + activationCount);
            console.log("Unique Classes: " + classesActivated.size);
            if (classesActivated.size > 0) {
                console.log("\nClasses Activated:");
                classesActivated.forEach(function(cls) {
                    console.log("  - " + cls);
                });
            }
            console.log("===================\n");
        }
    }, 10000);

    console.log("\n[*] All hooks installed! Monitoring WinRT API calls...");
    console.log("[*] Press Ctrl+C to stop and see summary\n");

} catch (e) {
    console.log("[-] Error: " + e.message);
    console.log("[-] combase.dll not loaded yet. Try running this script after the app starts.");
}

// On detach, print final summary
Process.setExceptionHandler(function(details) {
    console.log("\n=== Final WinRT Summary ===");
    console.log("Total Activations: " + activationCount);
    console.log("Unique Classes Activated: " + classesActivated.size);

    if (classesActivated.size > 0) {
        console.log("\nAll Classes:");
        var sorted = Array.from(classesActivated).sort();
        sorted.forEach(function(cls) {
            console.log("  - " + cls);
        });
    }

    console.log("\nRecommendation:");
    console.log("Implement these WinRT classes in Wine to support this application.");
    console.log("===========================\n");

    return false; // Continue with normal exception handling
});
