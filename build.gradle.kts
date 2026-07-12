plugins {
    id("com.android.library")
}

android {
    namespace = "top.mobilegl.mobileglues"
    compileSdk = 36

    defaultConfig {
        minSdk = 21
        ndkVersion = "27.3.13750724"
        ndk {
            abiFilters.add("arm64-v8a")
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
        }
        create("proguard") {
            isMinifyEnabled = true
            initWith(getByName("debug"))
        }
        create("fordebug") {
        }
    }

    externalNativeBuild {
        cmake {
            path = file("MobileGlues-cpp/CMakeLists.txt")
            version = "3.22.1"
            arguments.add("-DUSE_EXTRA_OPTIMIZATIONS=" + (System.getenv("USE_EXTRA_OPTIMIZATIONS") ?: "ON"))
            val ltoLevel = System.getenv("LTO_LEVEL")?.toIntOrNull() ?: 0
            val ltoFlag = when (ltoLevel) {
                2 -> "-flto"
                1 -> "-flto=thin"
                else -> ""
            }
            arguments.add("-DLTO_FLAG=" + ltoFlag)
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
}