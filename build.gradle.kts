plugins {
    id("com.android.library")
}

android {
    namespace = "top.mobilegl.mobileglues"
    compileSdk = 36

    defaultConfig {
        minSdk = 21

        ndkVersion = "27.3.13750724"
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
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
}
