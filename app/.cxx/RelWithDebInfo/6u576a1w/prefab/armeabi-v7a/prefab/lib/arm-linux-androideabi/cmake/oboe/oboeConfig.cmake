if(NOT TARGET oboe::oboe)
add_library(oboe::oboe SHARED IMPORTED)
set_target_properties(oboe::oboe PROPERTIES
    IMPORTED_LOCATION "C:/Users/kengr/.gradle/caches/8.14/transforms/eb21ab4bd5450522bea5cf78a559e04b/transformed/jetified-oboe-1.8.0/prefab/modules/oboe/libs/android.armeabi-v7a/liboboe.so"
    INTERFACE_INCLUDE_DIRECTORIES "C:/Users/kengr/.gradle/caches/8.14/transforms/eb21ab4bd5450522bea5cf78a559e04b/transformed/jetified-oboe-1.8.0/prefab/modules/oboe/include"
    INTERFACE_LINK_LIBRARIES ""
)
endif()

