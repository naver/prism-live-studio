add_subdirectory(QREncoder)
add_subdirectory(openssl)
add_subdirectory(MQTT)
add_subdirectory(QtApng) #TODO: mac need move to MacOS and signature
add_subdirectory(KSCrash)

if(OS_MACOS)
add_subdirectory(relaunch)
endif()

